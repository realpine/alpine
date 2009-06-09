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
#define STRICT
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>

/*
  article entitled "About Rich Edit Controls" here (remove all spaces from url):
    http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/
      platform/commctls/richedit/richeditcontrols/aboutricheditcontrols.asp

  Rich Edit version DLL
  1.0   Riched32.dll
  2.0   Riched20.dll
  3.0   Riched20.dll
  4.1   Msftedit.dll

  The following list describes which versions of Rich Edit are included in
    which releases of Microsoft Windows.
  Windows XP SP1  Includes Rich Edit 4.1, Rich Edit 3.0, and a Rich Edit 1.0 emulator.
  Windows XP      Includes Rich Edit 3.0 with a Rich Edit 1.0 emulator.
  Windows Me      Includes Rich Edit 1.0 and 3.0.
  Windows 2000    Includes Rich Edit 3.0 with a Rich Edit 1.0 emulator.
  Windows NT 4.0  Includes Rich Edit 1.0 and 2.0.
  Windows 98      Includes Rich Edit 1.0 and 2.0.
  Windows 95      Includes only Rich Edit 1.0. However, Riched20.dll is
                    compatible with Windows 95 and may be installed by an
                    application that requires it.

  We're using richedit v2 since it is the first to have Unicode support. Does
    potentially limit us to Win98 unless we install riched20.dll or it's
    already there.
 */
#define _RICHEDIT_VER   0x0200
#include <richedit.h>
#include "resource.h"

#include "mswin_tw.h"

/*
 * Globals
 */
static const TCHAR g_mswin_tw_class_name[] = TEXT("PineTWClass");

// Maximum amount of text allowed in these textwindows.
//  Set via a EM_EXLIMITTEXT message.
static const LPARAM g_max_text = 8 * 1024 * 1024 - 1;

/*
 * Function prototypes
 */
static LRESULT CALLBACK mswin_tw_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static UINT Edit_ExSetSel(HWND hwnd_edit, LONG cpMin, LONG cpMax);
static UINT Edit_ExGetTextLen(HWND hwnd_edit, DWORD flags);
static BOOL Edit_ExIsReadOnly(HWND hwnd_edit);

/*
 * mswin_tw_create() - Create a mswin textwindow.
 */
int
mswin_tw_create(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR title)
{
    int width, height;
    static int s_mswin_tw_class_registered = 0;

    mswin_tw->hwnd = NULL;
    mswin_tw->hwnd_edit = NULL;

    if(!s_mswin_tw_class_registered)
    {
        WNDCLASS  wndclass;

        LoadLibrary(TEXT("riched20.dll"));

        memset(&wndclass, 0, sizeof(wndclass));
        wndclass.style =         CS_BYTEALIGNWINDOW;
        wndclass.lpfnWndProc =   mswin_tw_wndproc;
        wndclass.cbClsExtra =    0;
        wndclass.cbWndExtra =    sizeof(ULONG_PTR);
        wndclass.hInstance =     mswin_tw->hInstance ;
        wndclass.hIcon =         LoadIcon (mswin_tw->hInstance, MAKEINTRESOURCE( ALPINEICON));
        wndclass.hCursor =       LoadCursor (NULL, IDC_ARROW);
        wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wndclass.lpszMenuName =  MAKEINTRESOURCE(TEXTWINMENU);
        wndclass.lpszClassName = g_mswin_tw_class_name ;

        RegisterClass(&wndclass);

        s_mswin_tw_class_registered = 1;
    }

    // If we're passed CW_USEDEFAULT as the position, then use right/bottom
    //  as the width/height. Otherwise use the full rect.
    width = (mswin_tw->rcSize.left == CW_USEDEFAULT) ?
        mswin_tw->rcSize.right : mswin_tw->rcSize.right - mswin_tw->rcSize.left;
    height = (mswin_tw->rcSize.top == CW_USEDEFAULT) ?
        mswin_tw->rcSize.bottom : mswin_tw->rcSize.bottom - mswin_tw->rcSize.top;

    mswin_tw->hwnd = CreateWindow(
                g_mswin_tw_class_name, title,
                WS_OVERLAPPEDWINDOW,
                mswin_tw->rcSize.left,
                mswin_tw->rcSize.top,
                width,
                height,
                HWND_DESKTOP, NULL,
                mswin_tw->hInstance, mswin_tw);
    if(!mswin_tw->hwnd)
        return 0;

    return 1;
}

/*
 * mswin_tw_close() - close the mswin textwindow.
 */
void
mswin_tw_close(MSWIN_TEXTWINDOW *mswin_tw)
{
    if(mswin_tw && mswin_tw->hwnd)
        DestroyWindow(mswin_tw->hwnd);
}

/*
 * EditStreamOutCallback - callback for mswin_tw_write_to_file.
 */
static DWORD CALLBACK
EditStreamOutCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb)
{
    HANDLE hFile = (HANDLE)dwCookie;
    return !WriteFile(hFile, lpBuff, cb, (DWORD *)pcb, NULL);
}

/*
 * mswin_tw_fill_from_file() - write textwindow contents to a file.
 */
BOOL
mswin_tw_write_to_file(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR file)
{
    BOOL fSuccess = FALSE;
    HANDLE hFile = CreateFile(file, GENERIC_WRITE, 0,
                              0, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        EDITSTREAM es;

        es.dwCookie = (DWORD_PTR)hFile;
        es.dwError = 0;
        es.pfnCallback = EditStreamOutCallback;

        if(SendMessage(mswin_tw->hwnd_edit, EM_STREAMOUT, SF_TEXT, (LPARAM)&es) &&
            es.dwError == 0)
        {
            fSuccess = TRUE;
        }
    }

    CloseHandle(hFile);
    return fSuccess;
}


#ifdef ALTED_DOT

/*
 * EditStreamCallback - callback for mswin_tw_fill_from_file.
 */
static DWORD CALLBACK
EditStreamInCallback(DWORD_PTR dwCookie, LPBYTE lpBuff, LONG cb, PLONG pcb)
{
    HANDLE hFile = (HANDLE)dwCookie;
    return !ReadFile(hFile, lpBuff, cb, (DWORD *)pcb, NULL);
}


/*
 * mswin_tw_fill_from_file() - read file contents into the mswin textwindow.
 */
BOOL
mswin_tw_fill_from_file(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR file)
{
    BOOL fSuccess = FALSE;
    HANDLE hFile = CreateFile(file, GENERIC_READ, FILE_SHARE_READ,
                              0, OPEN_EXISTING,
                              FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if(hFile != INVALID_HANDLE_VALUE)
    {
        EDITSTREAM es;

        es.dwCookie = (DWORD_PTR)hFile;
        es.dwError = 0;
        es.pfnCallback = EditStreamInCallback;

        if(SendMessage(mswin_tw->hwnd_edit, EM_STREAMIN, SF_TEXT, (LPARAM)&es) &&
            es.dwError == 0)
        {
            fSuccess = TRUE;
        }
    }

    CloseHandle(hFile);
    return fSuccess;
}

#endif /* ALTED_DOT */

/*
 * mswin_tw_showwindow() - show the main hwnd (SW_SHOWNORMAL, etc.).
 */
void
mswin_tw_showwindow(MSWIN_TEXTWINDOW *mswin_tw, int nCmdShow)
{
    if(mswin_tw && mswin_tw->hwnd)
        ShowWindow(mswin_tw->hwnd, nCmdShow);
}

/*
 * mswin_tw_setfont() - Sets the hfont for the entire edit control.
 */
void
mswin_tw_setfont(MSWIN_TEXTWINDOW *mswin_tw, HFONT hfont)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
        SetWindowFont(mswin_tw->hwnd_edit, hfont, TRUE);
}

/*
 * mswin_tw_setcolor() - Set colors for entire edit control. If we're
 *  passed -1 for mswin_tw, then set the colors for all textwindows.
 */
void
mswin_tw_setcolor(MSWIN_TEXTWINDOW *mswin_tw,
    COLORREF TextColor, COLORREF BackColor)
{
    if(mswin_tw == (MSWIN_TEXTWINDOW *)-1)
    {
        HWND hwnd = NULL;

        while(hwnd = FindWindowEx(NULL, hwnd, g_mswin_tw_class_name, NULL))
        {
            mswin_tw = (MSWIN_TEXTWINDOW *)(LONG_PTR)GetWindowLongPtr(
                hwnd, GWLP_USERDATA);
            if(mswin_tw)
            {
                mswin_tw_setcolor(mswin_tw, TextColor, BackColor);
            }
        }
    }
    else if(mswin_tw && mswin_tw->hwnd_edit)
    {
        CHARFORMAT2W cf2;

        memset(&cf2, 0, sizeof(cf2));
        cf2.cbSize = sizeof(cf2);
        cf2.dwMask = CFM_COLOR;
        cf2.crTextColor = TextColor;

        SendMessage(mswin_tw->hwnd_edit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf2);
        SendMessage(mswin_tw->hwnd_edit, EM_SETBKGNDCOLOR, 0, BackColor);
    }
}

/*
 * mswin_tw_puts_lptstr() - Stuffs a LPTSTR string at the end of our edit
 *  control.
 */
int
mswin_tw_puts_lptstr(MSWIN_TEXTWINDOW *mswin_tw, LPTSTR msg)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
    {
        TCHAR lf_str[] = TEXT("\n");

        Edit_ExSetSel(mswin_tw->hwnd_edit, -1, -1);
        Edit_ReplaceSel(mswin_tw->hwnd_edit, msg);

        Edit_ReplaceSel(mswin_tw->hwnd_edit, lf_str);

        Edit_ScrollCaret(mswin_tw->hwnd_edit);
        return 1;
    }

    return 0;
}

/*
 * mswin_tw_printf() - printf version of mswin_tw_puts_lptstr.
 */
int
mswin_tw_printf(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR fmt, ...)
{
    TCHAR msg[1024];
    va_list  vlist;

    va_start(vlist, fmt);
    _vsntprintf(msg, ARRAYSIZE(msg), fmt, vlist);
    va_end(vlist);

    msg[ARRAYSIZE(msg) - 1] = 0;
    return mswin_tw_puts_lptstr(mswin_tw, msg);
}

/*
 * mswin_tw_gettextlength() - Returns the number of TCHARs in the edit control.
 */
UINT
mswin_tw_gettextlength(MSWIN_TEXTWINDOW *mswin_tw)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
        return (UINT)Edit_ExGetTextLen(mswin_tw->hwnd_edit, GTL_USECRLF);

    return 0;
}

/*
 * mswin_tw_gettext() - Return value is the number of TCHARs copied into the
 *  output buffer.
 */
UINT
mswin_tw_gettext(MSWIN_TEXTWINDOW *mswin_tw, LPTSTR lptstr_ret, int lptstr_len)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
    {
        GETTEXTEX gt;
        LRESULT lresult;

        gt.cb = lptstr_len * sizeof(TCHAR);
        gt.flags = GT_DEFAULT | GT_USECRLF;
        gt.codepage = 1200;
        gt.lpDefaultChar = NULL;
        gt.lpUsedDefChar = NULL;

        lptstr_ret[0] = 0;
        lresult = SendMessage(mswin_tw->hwnd_edit, EM_GETTEXTEX,
            (WPARAM)&gt, (LPARAM)lptstr_ret);
        return (int)lresult;
    }

    return 0;
}

/*
 * mswin_tw_setsel() - Set edit control selection.
 */
void
mswin_tw_setsel(MSWIN_TEXTWINDOW *mswin_tw, LONG min, LONG max)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
    {
        Edit_ExSetSel(mswin_tw->hwnd_edit, min, max);
    }
}

/*
 * mswin_set_readonly() - Set readonly status.
 */
void
mswin_set_readonly(MSWIN_TEXTWINDOW *mswin_tw, BOOL read_only)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
        Edit_SetReadOnly(mswin_tw->hwnd_edit, read_only);
}

/*
 * mswin_tw_clear() - Clear all text from edit control.
 */
void
mswin_tw_clear(MSWIN_TEXTWINDOW *mswin_tw)
{
    if(mswin_tw && mswin_tw->hwnd_edit)
    {
        SETTEXTEX stex;

        stex.flags = ST_DEFAULT;
        stex.codepage = 1200;       // Unicode (see richedit.h)

        SendMessage(mswin_tw->hwnd_edit, EM_SETTEXTEX,
            (WPARAM)&stex, (LPARAM)TEXT(""));

        if(mswin_tw->clear_callback)
            mswin_tw->clear_callback(mswin_tw);
    }
}

/*
 * MySetWindowLongPtr() - Little wrapper routine which calls the
 *  Windows SetWindowLongPtr() and removes the stupid warning which is
 *  just seriously lame.
 */
static LONG_PTR
MySetWindowLongPtr(HWND hwnd, int nIndex, void *NewLongPtr)
{
// warning C4244: 'function': conversion from 'LONG_PTR' to 'LONG',
//  possible loss of data
#pragma warning(push)
#pragma warning(disable: 4244)
    return SetWindowLongPtr(hwnd, nIndex, (LONG_PTR)NewLongPtr);
#pragma warning(pop)
}

/*
 * mswin_tw_wm_command() - WM_CONTEXTMENU handler for textwindows
 */
static void
mswin_tw_wm_contextmenu(MSWIN_TEXTWINDOW *mswin_tw, HWND hwnd, HWND hwndContext,
    int xPos, int yPos)
{
   HMENU hMenu;

    hMenu = CreatePopupMenu();
    if(hMenu)
    {
        int i;
        CHARRANGE cr;
        MENUITEMINFO mitem;
        static const struct
        {
            UINT wID;
            LPTSTR dwTypeData;
        } s_popup_menu[] =
        {
            { IDM_EDIT_COPY,        TEXT("&Copy")        },
            { IDM_EDIT_COPY_APPEND, TEXT("Copy &Append") },
            { IDM_EDIT_CLEAR,       TEXT("Clea&r")       },
        };

        memset(&mitem, 0, sizeof(MENUITEMINFO));
        mitem.cbSize = sizeof(MENUITEMINFO);
        mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mitem.fType = MFT_STRING;

        SendMessage(mswin_tw->hwnd_edit, EM_EXGETSEL, (WPARAM)0, (LPARAM)&cr);

        for(i = 0; i < ARRAYSIZE(s_popup_menu); i++)
        {
            switch(s_popup_menu[i].wID)
            {
            case IDM_EDIT_CLEAR:
                // Only enable it if there is a clear callback set.
                mitem.fState = (mswin_tw->clear_callback) ? 
                    MFS_ENABLED : MFS_GRAYED;
                break;
            case IDM_EDIT_COPY:
            case IDM_EDIT_COPY_APPEND:
                // Only enable if there is a selection.
                mitem.fState = (cr.cpMax > cr.cpMin) ? 
                    MFS_ENABLED : MFS_GRAYED;
                break;
            default:
                mitem.fState = MFS_ENABLED;
                break;
            }
            
            mitem.wID = s_popup_menu[i].wID;
            mitem.dwTypeData = s_popup_menu[i].dwTypeData;
            mitem.cch = (UINT)_tcslen(s_popup_menu[i].dwTypeData);
            InsertMenuItem(hMenu, i, FALSE, &mitem);
        }

        TrackPopupMenu(hMenu,
               TPM_LEFTALIGN | TPM_TOPALIGN |
               TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
               xPos, yPos, 0, hwnd, NULL);

        DestroyMenu(hMenu);
    }
}

/*
 * mswin_tw_wm_command() - WM_COMMAND handler for textwindows
 */
static void
mswin_tw_wm_command(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    MSWIN_TEXTWINDOW *mswin_tw;

    mswin_tw = (MSWIN_TEXTWINDOW *)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch(id)
    {
    case IDM_FILE_CLOSE:
        DestroyWindow(hwnd);
        break;

    case IDM_FILE_PRINT:
        if(mswin_tw->print_callback)
            mswin_tw->print_callback(mswin_tw);
        break;

    case IDM_EDIT_COPY:
        SendMessage(mswin_tw->hwnd_edit, WM_COPY, 0, 0);
        break;

    case IDM_EDIT_CLEAR:
        mswin_tw_clear(mswin_tw);
        break;

    case IDM_EDIT_COPY_APPEND:
    {
        CHARRANGE cr;
        int text_len0, text_len1;
        HWND hwnd_edit = mswin_tw->hwnd_edit;
        BOOL EditIsReadOnly = Edit_ExIsReadOnly(hwnd_edit);

        SetWindowRedraw(hwnd_edit, FALSE);
        Edit_SetReadOnly(hwnd_edit, FALSE);

        // Get current selection.
        SendMessage(hwnd_edit, EM_EXGETSEL, (WPARAM)0, (LPARAM)&cr);

        // Get current length.
        text_len0 = Edit_ExGetTextLen(hwnd_edit, 0);

        // Paste current clip right before our new selection.
        Edit_ExSetSel(hwnd_edit, cr.cpMin, cr.cpMin);
        SendMessage(hwnd_edit, WM_PASTE, 0, 0);

        // Get new length.
        text_len1 = Edit_ExGetTextLen(hwnd_edit, 0);

        // Select new and old clip and copy em.
        Edit_ExSetSel(hwnd_edit, cr.cpMin, cr.cpMax + text_len1 - text_len0);
        SendMessage(hwnd_edit, WM_COPY, 0, 0);

        // Undo our paste and restore original selection.
        SendMessage(hwnd_edit, WM_UNDO, 0, 0);
        SendMessage(hwnd_edit, EM_EXSETSEL, (WPARAM)0, (LPARAM)&cr);

        // Set back to read only.
        Edit_SetReadOnly(hwnd_edit, EditIsReadOnly);
        SetWindowRedraw(hwnd_edit, TRUE);
        break;
    }

    case IDM_EDIT_SEL_ALL:
        Edit_ExSetSel(mswin_tw->hwnd_edit, 0, -1);
        break;
    }
}

/*
 * mswin_tw_wm_notify() - WM_NOTIFY handler for textwindows
 */
static LRESULT
mswin_tw_wm_notify(HWND hwnd, int idCtrl, NMHDR *nmhdr)
{
    HWND hwnd_edit = nmhdr->hwndFrom;

    if(nmhdr->code == EN_MSGFILTER)
    {
        MSGFILTER *msg_filter = (MSGFILTER *)nmhdr;

        if(msg_filter->msg == WM_KEYDOWN)
        {
            if(msg_filter->wParam == 'E')
            {
                int control_down = GetKeyState(VK_CONTROL) < 0;
                int shift_down = GetKeyState(VK_SHIFT) < 0;

                // Ctrl+Shift+E toggles the readonly attribute on the text
                //  buffer.
                if(control_down && shift_down)
                    Edit_SetReadOnly(hwnd_edit, !Edit_ExIsReadOnly(hwnd_edit));
                return TRUE;
            }
        }
        else if(msg_filter->msg == WM_CHAR)
        {
            // Only override these keys if this buffer is readonly.
            if(Edit_ExIsReadOnly(hwnd_edit))
            {
                switch(msg_filter->wParam)
                {
                case 'k':
                    SendMessage(hwnd_edit, EM_SCROLL, SB_LINEUP, 0);
                    return TRUE;
                case 'j':
                    SendMessage(hwnd_edit, EM_SCROLL, SB_LINEDOWN, 0);
                    return TRUE;
                case '-':
                case 'b':
                    SendMessage(hwnd_edit, EM_SCROLL, SB_PAGEUP, 0);
                    return TRUE;
                case ' ':
                case 'f':
                    SendMessage(hwnd_edit, EM_SCROLL, SB_PAGEDOWN, 0);
                    return TRUE;
                }
            }
        }
    }
    else if(nmhdr->code == EN_LINK)
    {
        ENLINK *enlink = (ENLINK *)nmhdr;

        if(enlink->msg == WM_LBUTTONDOWN)
        {
            TEXTRANGE tr;
            TCHAR link_buf[1024];

            link_buf[0] = 0;
            tr.lpstrText = link_buf;

            tr.chrg = enlink->chrg;
            if(tr.chrg.cpMax - tr.chrg.cpMin > ARRAYSIZE(link_buf))
                tr.chrg.cpMax = tr.chrg.cpMin + ARRAYSIZE(link_buf);

            SendMessage(hwnd_edit, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
            ShellExecute(hwnd, TEXT("Open"), link_buf, NULL,  NULL,  SW_SHOWNORMAL);
            return TRUE;
        }
    }

    return FALSE;
}

/*
 * mswin_tw_wndproc() - Main window proc for mswin textwindows.
 */
static LRESULT CALLBACK
mswin_tw_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MSWIN_TEXTWINDOW *mswin_tw;

    mswin_tw = (MSWIN_TEXTWINDOW *)(LONG_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch(msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *pcs = (CREATESTRUCT *)lp;

        mswin_tw = (MSWIN_TEXTWINDOW *)pcs->lpCreateParams;

        MySetWindowLongPtr(hwnd, GWLP_USERDATA, mswin_tw);

        mswin_tw->hwnd_edit = CreateWindowEx(
                        WS_EX_CLIENTEDGE, RICHEDIT_CLASS, 0,
                        WS_VISIBLE | WS_CHILD |
                        WS_CLIPSIBLINGS | WS_CLIPCHILDREN |
                        WS_VSCROLL | WS_HSCROLL |
                        ES_MULTILINE | ES_READONLY | ES_NOHIDESEL,
                        0, 0, 1, 1,
                        hwnd, 0, mswin_tw->hInstance, 0);

        // We want link and key event notifications.
        SendMessage(mswin_tw->hwnd_edit, EM_SETEVENTMASK,
            0, (ENM_KEYEVENTS | ENM_LINK));

        // Specifies the maximum amount of text that can be entered.
        SendMessage(mswin_tw->hwnd_edit, EM_EXLIMITTEXT, 0, g_max_text);

        // Enable automatic detection of URLs by our rich edit control.
        SendMessage(mswin_tw->hwnd_edit, EM_AUTOURLDETECT, TRUE, 0);
        break;
    }

    case WM_CONTEXTMENU:
        mswin_tw_wm_contextmenu(mswin_tw, hwnd, (HWND)wp, GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        break;

    case WM_NOTIFY:
        return mswin_tw_wm_notify(hwnd, (int)wp, (NMHDR *)lp);

    case WM_COMMAND:
        HANDLE_WM_COMMAND(hwnd, wp, lp, mswin_tw_wm_command);
        break;

    case WM_SETFOCUS:
        SetFocus(mswin_tw->hwnd_edit);
        return TRUE;

    case WM_SIZE:
        MoveWindow(mswin_tw->hwnd_edit, 0, 0, LOWORD(lp), HIWORD(lp), TRUE);
        break;

    case WM_WINDOWPOSCHANGED:
        if(!IsIconic(hwnd))
        {
            WINDOWPOS *wpos = (WINDOWPOS *)lp;

            mswin_tw->rcSize.left = wpos->x;
            mswin_tw->rcSize.top = wpos->y;
            mswin_tw->rcSize.right = wpos->x + wpos->cx;
            mswin_tw->rcSize.bottom = wpos->y + wpos->cy;
        }
        break;

    case WM_DESTROY:
        if(mswin_tw->out_file)
        {
            mswin_tw->out_file_ret = mswin_tw_write_to_file(mswin_tw,
                mswin_tw->out_file);
        }

        mswin_tw->hwnd = NULL;
        mswin_tw->hwnd_edit = NULL;

        if(mswin_tw->close_callback)
            mswin_tw->close_callback(mswin_tw);
        return TRUE;

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/*
 * Edit_ExGetTextLen() - Helper routine for getting count of chars.
 */
static UINT
Edit_ExGetTextLen(HWND hwnd_edit, DWORD flags)
{
    GETTEXTLENGTHEX gtl;

    gtl.flags = GTL_PRECISE | GTL_NUMCHARS | flags;
    gtl.codepage = 1200;        // Unicode (see richedit.h)
    return (UINT)SendMessage(hwnd_edit, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
}

/*
 * Edit_ExSetSel() - Helper routine for setting edit selection.
 */
static UINT
Edit_ExSetSel(HWND hwnd_edit, LONG cpMin, LONG cpMax)
{
    CHARRANGE cr;

    if(cpMin == -1)
        cpMin = Edit_ExGetTextLen(hwnd_edit, 0);

    cr.cpMin = cpMin;
    cr.cpMax = cpMax;
    return (UINT)SendMessage(hwnd_edit, EM_EXSETSEL, (WPARAM)0, (LPARAM)&cr);
}

/*
 * Edit_ExIsReadOnly() - TRUE if edit buffer is read only.
 */
static BOOL
Edit_ExIsReadOnly(HWND hwnd_edit)
{
    LRESULT edit_opts;

    edit_opts = SendMessage(hwnd_edit, EM_GETOPTIONS, 0, 0);
    return !!(edit_opts & ECO_READONLY);
}

