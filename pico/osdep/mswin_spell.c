/*
 * ========================================================================
 * MSWIN_SPELL.C
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#include "../headers.h"

#include "mswin_aspell.h"
#include "mswin_spell.h"

#include <windowsx.h>

typedef struct WORD_INFO
{
    ASPELLINFO *aspellinfo;         // Aspell information

    const char *utf8_err_message;   // Error message, if any

    char       *word_utf8;          // utf-8
    LPTSTR      word_lptstr;

    int         word_doto;          // UCS
    LINE       *word_dotp;          //
    int         word_size;          //
} WORD_INFO;

extern HINSTANCE        ghInstance;
extern HWND             ghTTYWnd;

/*
 * Function prototypes
 */
static int get_next_bad_word(WORD_INFO *word_info);
static void free_word_info_words(WORD_INFO *word_info);
static INT_PTR CALLBACK spell_dlg_proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

/*
 * spell() - check for potentially missspelled words and offer them for
 *      correction. Microsoft Windows specific version.
 */
int
spell(int f, int n)
{
    int w_marko_bak;
    LINE *w_markp_bak;
    WORD_INFO word_info;
    ASPELLINFO *aspellinfo;

    emlwrite(_("Checking spelling..."), NULL);  /* greetings! */

    memset(&word_info, 0, sizeof(WORD_INFO));

    aspellinfo = speller_init(NULL);
    if(!aspellinfo ||
        (word_info.utf8_err_message = speller_get_error_message(aspellinfo)))
    {
        if(word_info.utf8_err_message)
            emlwrite((char *)word_info.utf8_err_message, NULL);  /* sad greetings! */
        else
            emlwrite(_("Spelling: initializing Aspell failed"), NULL);

        speller_close(aspellinfo);
        return -1;
    }

    // Back up current mark.
    w_marko_bak = curwp->w_marko;
    w_markp_bak = curwp->w_markp;

    setimark(0, 1);
    gotobob(0, 1);

    word_info.aspellinfo = aspellinfo;

    if(get_next_bad_word(&word_info))
    {
        DialogBoxParam(ghInstance, MAKEINTRESOURCE(DLG_SPELL), ghTTYWnd,
            spell_dlg_proc, (LPARAM)&word_info);
    }

    // Restore original mark if possible.
    if(curwp->w_markp)
    {
        // Clear any set marks.
        setmark(0, 1);

        if(w_markp_bak && llength(w_markp_bak))
        {
            // Make sure we don't set mark off the line.
            if(w_marko_bak >= llength(w_markp_bak))
                w_marko_bak = llength(w_markp_bak) - 1;

            curwp->w_marko = w_marko_bak;
            curwp->w_markp = w_markp_bak;
        }
    }

    if(word_info.utf8_err_message)
        emlwrite((char *)word_info.utf8_err_message, NULL);
    else
        emlwrite(_("Done checking spelling"), NULL);

    speller_close(aspellinfo);
    free_word_info_words(&word_info);

    swapimark(0, 1);
    curwp->w_flag |= WFHARD|WFMODE;

    update();
    return 1;
}

/*
 * handle_cb_editchange() - combobox contents have changed so enable/disable
 *      the Change and Change All buttons accordingly.
 */
static void
handle_cb_editchange(HWND hDlg, HWND hwnd_combo)
{
    int text_len = ComboBox_GetTextLength(hwnd_combo);

    Button_Enable(GetDlgItem(hDlg, ID_BUTTON_CHANGE), !!text_len);
    Button_Enable(GetDlgItem(hDlg, ID_BUTTON_CHANGEALL), !!text_len);
}

/*
 * spell_dlg_proc_set_word() - Set up the dialog to display spelling
 *      information and suggestions for the current word.
 */
static void
spell_dlg_proc_set_word(HWND hDlg, WORD_INFO *word_info)
{
    TCHAR dlg_title[256];
    HWND hwnd_combo = GetDlgItem(hDlg, ID_COMBO_SUGGESTIONS);

    // Clear the combobox.
    ComboBox_ResetContent(hwnd_combo);

    // Set the dialog box title
    _sntprintf(dlg_title, ARRAYSIZE(dlg_title), TEXT("Not in Dictionary: %s"),
        word_info->word_lptstr);
    dlg_title[ARRAYSIZE(dlg_title) - 1] = 0;
    SetWindowText(hDlg, dlg_title);

    // Populate the combobox with suggestions
    if(speller_suggestion_init(word_info->aspellinfo, word_info->word_utf8, -1))
    {
        const char *suggestion_utf8;

        while(suggestion_utf8 = speller_suggestion_getnext(word_info->aspellinfo))
        {
            LPTSTR suggestion_lptstr = utf8_to_lptstr((LPSTR)suggestion_utf8);

            if(suggestion_lptstr)
            {
                ComboBox_AddString(hwnd_combo, suggestion_lptstr);
                fs_give((void **)&suggestion_lptstr);
            }
        }

        // Select the first one.
        ComboBox_SetCurSel(hwnd_combo, 0);

        speller_suggestion_close(word_info->aspellinfo);
    }

    handle_cb_editchange(hDlg, hwnd_combo);
}

/*
 * spell_dlg_next_word() - Move to the next misspelled word and display
 *      the information for it. If no more bad words, kill the dialog.
 */
static void
spell_dlg_next_word(HWND hDlg, WORD_INFO *word_info)
{
    if(!get_next_bad_word(word_info))
    {
        EndDialog(hDlg, 0);
        return;
    }

    spell_dlg_proc_set_word(hDlg, word_info);
}

/*
 * chword_n() - change the given word_len pointed to by the curwp->w_dot
 *      pointers to the word in cb
 */
static void
chword_n(int wb_len, UCS *cb)
{
    ldelete(wb_len, NULL);  /* not saved in kill buffer */

    while(*cb != '\0')
        linsert(1, *cb++);

    curwp->w_flag |= WFEDIT;
}

/*
 * replace_all() - replace all instances of oldword with newword from
 *      the current '.' on. Mark is where curwp will get restored to.
 */
static void
replace_all(UCS *oldword, int oldword_len, UCS *newword)
{
    int             wrap;

    // Mark should be at "." right now - ie the start of the word we're
    //  checking. We're going to use mark below to restore curwp since
    //  chword_n can potentially realloc LINES and it knows about markp.
    assert(curwp->w_markp);
    assert(curwp->w_markp == curwp->w_dotp);
    assert(curwp->w_marko == curwp->w_doto);

    curwp->w_bufp->b_mode |= MDEXACT;           /* case sensitive */
    while(forscan(&wrap, oldword, NULL, 0, 1))
    {
        if(wrap)
            break;                              /* wrap NOT allowed! */

        /*
         * We want to minimize the number of substrings that we report
         * as matching a misspelled word...
         */
        if(lgetc(curwp->w_dotp, 0).c != '>')
        {
            LINE *lp  = curwp->w_dotp;          /* for convenience */
            int   off = curwp->w_doto;

            if(off == 0 || !ucs4_isalpha(lgetc(lp, off - 1).c))
            {
                off += oldword_len;

                if(off == llength(lp) || !ucs4_isalpha(lgetc(lp, off).c))
                {
                    // Replace this word
                    chword_n(oldword_len, newword);
                }
            }
        }

        forwchar(0, 1);                         /* move on... */

    }
    curwp->w_bufp->b_mode ^= MDEXACT;           /* case insensitive */

    curwp->w_dotp = curwp->w_markp;
    curwp->w_doto = curwp->w_marko;
}

/*
 * handle_button_change() - Someone pressed the Change or Change All button.
 *      So deal with it.
 */
static void
handle_button_change(HWND hDlg, WORD_INFO *word_info, int do_replace_all)
{
    int str_lptstr_len;
    TCHAR str_lptstr[128];

    // Get the length of what they want to change to.
    str_lptstr_len = ComboBox_GetText(GetDlgItem(hDlg, ID_COMBO_SUGGESTIONS),
        str_lptstr, ARRAYSIZE(str_lptstr));
    if(str_lptstr_len)
    {
        UCS *str_ucs4;
        char *str_utf8;

        // Notify the speller so it'll suggest this word next time around.
        str_utf8 = lptstr_to_utf8(str_lptstr);
        if(str_utf8)
        {
            speller_replace_word(word_info->aspellinfo,
                word_info->word_utf8, -1, str_utf8, -1);
            fs_give((void **)&str_utf8);
        }

        str_ucs4 = lptstr_to_ucs4(str_lptstr);
        if(!str_ucs4)
        {
            // Uh oh - massive error.
            word_info->utf8_err_message = _("Spelling: lptstr_to_ucs4() failed");
            EndDialog(hDlg, -1);
            return;
        }

        // Move back to start of this word.
        curwp->w_doto = word_info->word_doto;
        curwp->w_dotp = word_info->word_dotp;

        if(do_replace_all)
        {
            int i;
            int word_size;
            UCS word[128];

            word_size = word_info->word_size;
            if(word_size >= ARRAYSIZE(word) - 1)
                word_size = ARRAYSIZE(word) - 1;

            for(i = 0; i < word_size; i++)
                word[i] = lgetc(curwp->w_dotp, curwp->w_doto + i).c;

            word[i] = 0;

            replace_all(word, word_size, str_ucs4);

            // Skip over the word we just inserted.
            forwchar(FALSE, (int)ucs4_strlen(str_ucs4));
        }
        else
        {
            // Swap it with the new improved version.
            chword_n(word_info->word_size, str_ucs4);
        }
        fs_give((void **)&str_ucs4);
    }

    update();
    spell_dlg_next_word(hDlg, word_info);
}

/*
 * center_dialog() - Center a dialog with respect to its parent.
 */
static void
center_dialog(HWND hDlg)
{
    int dx;
    int dy;
    RECT rcDialog;
    RECT rcParent;

    GetWindowRect(GetParent(hDlg), &rcParent);
    GetWindowRect(hDlg, &rcDialog);

    dx = ((rcParent.right - rcParent.left) -
            (rcDialog.right - rcDialog.left)) / 2 + rcParent.left - rcDialog.left;

    dy = ((rcParent.bottom - rcParent.top) -
            (rcDialog.bottom - rcDialog.top)) / 2 + rcParent.top - rcDialog.top;

    OffsetRect(&rcDialog, dx, dy);

    SetWindowPos(hDlg, NULL, rcDialog.left, rcDialog.top, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER);
}

/*
 * spell_dlg_on_command() - Handle the WM_COMMAND stuff for the spell dialog.
 */
static void
spell_dlg_on_command(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
    WORD_INFO *word_info =
        (WORD_INFO *)(LONG_PTR)GetWindowLongPtr(hDlg, GWLP_USERDATA);

    switch(id)
    {
    case ID_COMBO_SUGGESTIONS:
        if(codeNotify == CBN_EDITCHANGE)
            handle_cb_editchange(hDlg, hwndCtl);
        break;

    case ID_BUTTON_CHANGE:
        handle_button_change(hDlg, word_info, 0);
        break;

    case ID_BUTTON_CHANGEALL:
        handle_button_change(hDlg, word_info, 1);
        break;

    case ID_BUTTON_IGNOREONCE:
        spell_dlg_next_word(hDlg, word_info);
        break;

    case ID_BUTTON_IGNOREALL:
        speller_ignore_all(word_info->aspellinfo, word_info->word_utf8, -1);
        spell_dlg_next_word(hDlg, word_info);
        break;

    case ID_BUTTON_ADD_TO_DICT:
        speller_add_to_dictionary(word_info->aspellinfo, word_info->word_utf8, -1);
        spell_dlg_next_word(hDlg, word_info);
        break;

    case ID_BUTTON_CANCEL:
        EndDialog(hDlg, 0);
        break;
    }
}

/*
 * spell_dlg_proc() - Main spell dialog proc.
 */
static INT_PTR CALLBACK
spell_dlg_proc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    WORD_INFO *word_info;

    switch(message)
    {
    case WM_INITDIALOG:
        center_dialog(hDlg);

        // Limit how much junk they can type in.
        ComboBox_LimitText(GetDlgItem(hDlg, ID_COMBO_SUGGESTIONS), 128);

        word_info = (WORD_INFO *)lParam;
        SetWindowLongPtr(hDlg, GWLP_USERDATA, (LONG_PTR)word_info);

        spell_dlg_proc_set_word(hDlg, word_info);
        return TRUE;

    case WM_COMMAND:
        HANDLE_WM_COMMAND(hDlg, wParam, lParam, spell_dlg_on_command);
        return TRUE;
    }

    return FALSE;
}

/*
 * get_next_word() - Get the next word from dot. And skip words in
 *      quoted lines.
 */
static int
get_next_word(WORD_INFO *word_info)
{
    // Skip quoted lines
    while(lgetc(curwp->w_dotp, 0).c == '>')
    {
        if(curwp->w_dotp == curbp->b_linep)
            return 0;

        curwp->w_dotp  = lforw(curwp->w_dotp);
        curwp->w_doto  = 0;
        curwp->w_flag |= WFMOVE;
    }

    // Move to a word.
    while(inword() == FALSE)
    {
        if(forwchar(FALSE, 1) == FALSE)
        {
            // There is no next word.
            return 0;
        }
    }

    // If mark is currently set, clear it.
    if(curwp->w_markp)
        setmark(0, 1);

    // Set mark to beginning of this word.
    curwp->w_markp = curwp->w_dotp;
    curwp->w_marko = curwp->w_doto;

    // Get word length
    word_info->word_doto = curwp->w_doto;
    word_info->word_dotp = curwp->w_dotp;
    word_info->word_size = 0;

    while((inword() != FALSE) && (word_info->word_dotp == curwp->w_dotp))
    {
        word_info->word_size++;

        if(forwchar(FALSE, 1) == FALSE)
            break;
    }

    word_info->word_utf8 = ucs4_to_utf8_cpystr_n(
        (UCS *)&word_info->word_dotp->l_text[word_info->word_doto],
        word_info->word_size);
    return 1;
}

/*
 * get_next_word() - free the word_info members word_utf8 and word_lptstr.
 */
static void
free_word_info_words(WORD_INFO *word_info)
{
    if(word_info->word_utf8)
    {
        fs_give((void **)&word_info->word_utf8);
        word_info->word_utf8 = NULL;
    }

    if(word_info->word_lptstr)
    {
        fs_give((void **)&word_info->word_lptstr);
        word_info->word_lptstr = NULL;
    }
}

/*
 * get_next_bad_word() - Search from '.' for the next word we think is
 *      rotten. Mark it and highlight it.
 */
static int
get_next_bad_word(WORD_INFO *word_info)
{
    free_word_info_words(word_info);

    while(get_next_word(word_info))
    {
        int ret;

	/* pass over words that contain @ */
	if(strchr(word_info->word_utf8, '@'))
	  ret = 1;
	else
          ret = speller_check_word(word_info->aspellinfo, word_info->word_utf8, -1);

        if(ret == -1)
        {
            word_info->utf8_err_message =
                speller_get_error_message(word_info->aspellinfo);
            if(!word_info->utf8_err_message)
                word_info->utf8_err_message = _("Spelling: speller_check_word() failed");
            return 0;
        }
        else if(ret == 0)
        {
            // Highlight word.
            update();

            word_info->word_lptstr = utf8_to_lptstr(word_info->word_utf8);
            return 1;
        }

        free_word_info_words(word_info);
    }

    return 0;
}
