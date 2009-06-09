/*
 * ========================================================================
 * FILE: MSWIN_ASPELL.C
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */
#include <system.h>
#include "mswin_aspell.h"

/*
 * Aspell typedefs
 */
typedef struct AspellConfig AspellConfig;
typedef struct AspellSpeller AspellSpeller;
typedef struct AspellCanHaveError AspellCanHaveError;
typedef struct AspellStringEnumeration AspellStringEnumeration;
typedef struct AspellWordList AspellWordList;
typedef struct AspellError AspellError;

/*
 * Aspell function prototypes
 */
typedef AspellCanHaveError *     (__cdecl NEW_ASPELL_SPELLER)(AspellConfig * config);
typedef AspellConfig *           (__cdecl NEW_ASPELL_CONFIG)();
typedef AspellSpeller *          (__cdecl TO_ASPELL_SPELLER)(AspellCanHaveError * obj);
typedef int                      (__cdecl ASPELL_CONFIG_REPLACE)(AspellConfig * ths, const char * key, const char * value);
typedef void                     (__cdecl DELETE_ASPELL_CONFIG)(AspellConfig * ths);
typedef const char *             (__cdecl ASPELL_CONFIG_RETRIEVE)(AspellConfig * ths, const char * key);
typedef int                      (__cdecl ASPELL_SPELLER_ADD_TO_PERSONAL)(AspellSpeller * ths, const char * word, int word_size);
typedef int                      (__cdecl ASPELL_SPELLER_ADD_TO_SESSION)(AspellSpeller * ths, const char * word, int word_size);
typedef int                      (__cdecl ASPELL_SPELLER_CHECK)(AspellSpeller * ths, const char * word, int word_size);
typedef const AspellWordList *   (__cdecl ASPELL_SPELLER_SUGGEST)(AspellSpeller * ths, const char * word, int word_size);
typedef int                      (__cdecl ASPELL_SPELLER_SAVE_ALL_WORD_LISTS)(AspellSpeller * ths);
typedef int                      (__cdecl ASPELL_SPELLER_STORE_REPLACEMENT)(AspellSpeller * ths, const char * mis, int mis_size, const char * cor, int cor_size);
typedef const char *             (__cdecl ASPELL_STRING_ENUMERATION_NEXT)(AspellStringEnumeration * ths);
typedef AspellStringEnumeration *(__cdecl ASPELL_WORD_LIST_ELEMENTS)(const AspellWordList * ths);
typedef void                     (__cdecl DELETE_ASPELL_SPELLER)(AspellSpeller * ths);
typedef void                     (__cdecl DELETE_ASPELL_STRING_ENUMERATION)(AspellStringEnumeration * ths);
typedef void                     (__cdecl DELETE_ASPELL_CAN_HAVE_ERROR)(AspellCanHaveError * ths);
typedef const char *             (__cdecl ASPELL_ERROR_MESSAGE)(const AspellCanHaveError * ths);
typedef unsigned int             (__cdecl ASPELL_ERROR_NUMBER)(const AspellCanHaveError * ths);
typedef const AspellError *      (__cdecl ASPELL_SPELLER_ERROR)(const AspellSpeller * ths);
typedef const char *             (__cdecl ASPELL_SPELLER_ERROR_MESSAGE)(const struct AspellSpeller * ths);
typedef AspellConfig *           (__cdecl ASPELL_SPELLER_CONFIG)(AspellSpeller * ths);

/*
 * MsWin speller information structure
 */
typedef struct ASPELLINFO
{
    AspellSpeller                       *speller;
    AspellStringEnumeration             *suggestion_elements;
    const char                          *error_message;

    AspellCanHaveError                  *err;

    HMODULE                             mod_aspell;

    NEW_ASPELL_SPELLER                  *new_aspell_speller;
    NEW_ASPELL_CONFIG                   *new_aspell_config;
    TO_ASPELL_SPELLER                   *to_aspell_speller;
    ASPELL_CONFIG_REPLACE               *aspell_config_replace;
    DELETE_ASPELL_CONFIG                *delete_aspell_config;
    ASPELL_CONFIG_RETRIEVE              *aspell_config_retrieve;
    ASPELL_SPELLER_ADD_TO_PERSONAL      *aspell_speller_add_to_personal;
    ASPELL_SPELLER_ADD_TO_SESSION       *aspell_speller_add_to_session;
    ASPELL_SPELLER_CHECK                *aspell_speller_check;
    ASPELL_SPELLER_SUGGEST              *aspell_speller_suggest;
    ASPELL_SPELLER_SAVE_ALL_WORD_LISTS  *aspell_speller_save_all_word_lists;
    ASPELL_SPELLER_STORE_REPLACEMENT    *aspell_speller_store_replacement;
    ASPELL_STRING_ENUMERATION_NEXT      *aspell_string_enumeration_next;
    ASPELL_WORD_LIST_ELEMENTS           *aspell_word_list_elements;
    DELETE_ASPELL_SPELLER               *delete_aspell_speller;
    DELETE_ASPELL_STRING_ENUMERATION    *delete_aspell_string_enumeration;
    DELETE_ASPELL_CAN_HAVE_ERROR        *delete_aspell_can_have_error;
    ASPELL_ERROR_MESSAGE                *aspell_error_message;
    ASPELL_ERROR_NUMBER                 *aspell_error_number;
    ASPELL_SPELLER_ERROR                *aspell_speller_error;
    ASPELL_SPELLER_ERROR_MESSAGE        *aspell_speller_error_message;
    ASPELL_SPELLER_CONFIG               *aspell_speller_config;
} ASPELLINFO;

/*
 * Find, aspell-15.dll, load it, and attempt to initialize our func pointers.
 *  1:success, 0:failed
 */
static int
speller_load_aspell_library(ASPELLINFO *aspellinfo)
{
    HKEY hKey;
    int success = 1;
    HMODULE mod_aspell = NULL;
    TCHAR aspell_fullname[MAX_PATH + 1];
    static const TCHAR aspell_name[] = TEXT("aspell-15.dll");

    aspell_fullname[0] = '\0';

    // Try to open the Aspell Registry key.
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Aspell"),
                    0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dtype;
        TCHAR aspell_path[MAX_PATH + 1];
        DWORD aspell_path_len = sizeof(aspell_path);

        // Query the path.
        if(RegQueryValueEx(hKey, TEXT("Path"), NULL, &dtype,
            (LPBYTE)aspell_path, &aspell_path_len) == ERROR_SUCCESS)
        {
            _sntprintf(aspell_fullname, ARRAYSIZE(aspell_fullname),
                TEXT("%s\\%s"), aspell_path, aspell_name);
            aspell_fullname[ARRAYSIZE(aspell_fullname) - 1] = 0;
        }

        RegCloseKey(hKey);
    }

    // If the registry thing didn't work, then just try doing a Loadlibrary
    //  using the standard Windows LoadLibrary search gunk.
    if(!aspell_fullname[0])
    {
        _tcsncpy(aspell_fullname, aspell_name, ARRAYSIZE(aspell_fullname));
        aspell_fullname[ARRAYSIZE(aspell_fullname) - 1] = 0;
    }

    // Load the library.
    mod_aspell = LoadLibrary(aspell_fullname);
    if(!mod_aspell)
    {
        // Failed.
        success = 0;
    }
    else
    {
        // Found the library - now try to initialize our function pointers.
        //
#define GET_ASPELL_PROC_ADDR(_functype, _func)                                \
        aspellinfo->_func = (_functype *)GetProcAddress(mod_aspell, #_func);  \
        if(!aspellinfo->_func)                                                \
            success = 0

        GET_ASPELL_PROC_ADDR(NEW_ASPELL_SPELLER,                 new_aspell_speller);
        GET_ASPELL_PROC_ADDR(NEW_ASPELL_CONFIG,                  new_aspell_config);
        GET_ASPELL_PROC_ADDR(TO_ASPELL_SPELLER,                  to_aspell_speller);
        GET_ASPELL_PROC_ADDR(ASPELL_CONFIG_REPLACE,              aspell_config_replace);
        GET_ASPELL_PROC_ADDR(DELETE_ASPELL_CONFIG,               delete_aspell_config);
        GET_ASPELL_PROC_ADDR(ASPELL_CONFIG_RETRIEVE,             aspell_config_retrieve);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_ADD_TO_PERSONAL,     aspell_speller_add_to_personal);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_ADD_TO_SESSION,      aspell_speller_add_to_session);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_CHECK,               aspell_speller_check);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_SUGGEST,             aspell_speller_suggest);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_SAVE_ALL_WORD_LISTS, aspell_speller_save_all_word_lists);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_STORE_REPLACEMENT,   aspell_speller_store_replacement);
        GET_ASPELL_PROC_ADDR(ASPELL_STRING_ENUMERATION_NEXT,     aspell_string_enumeration_next);
        GET_ASPELL_PROC_ADDR(ASPELL_WORD_LIST_ELEMENTS,          aspell_word_list_elements);
        GET_ASPELL_PROC_ADDR(DELETE_ASPELL_SPELLER,              delete_aspell_speller);
        GET_ASPELL_PROC_ADDR(DELETE_ASPELL_STRING_ENUMERATION,   delete_aspell_string_enumeration);
        GET_ASPELL_PROC_ADDR(DELETE_ASPELL_CAN_HAVE_ERROR,       delete_aspell_can_have_error);
        GET_ASPELL_PROC_ADDR(ASPELL_ERROR_MESSAGE,               aspell_error_message);
        GET_ASPELL_PROC_ADDR(ASPELL_ERROR_NUMBER,                aspell_error_number);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_ERROR,               aspell_speller_error);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_ERROR_MESSAGE,       aspell_speller_error_message);
        GET_ASPELL_PROC_ADDR(ASPELL_SPELLER_CONFIG,              aspell_speller_config);

#undef GET_ASPELL_PROC_ADDR

        if(!success)
        {
            FreeLibrary(mod_aspell);
            mod_aspell = NULL;
        }
    }

    aspellinfo->mod_aspell = mod_aspell;
    return success;
}

/*
 * Find, load, and initialize the aspell library.
 *  NULL on failure, otherwise an ASPELLINFO pointer.
 */
ASPELLINFO *
speller_init(char *lang)
{
    ASPELLINFO *aspellinfo;

    aspellinfo = (ASPELLINFO *)malloc(sizeof(ASPELLINFO));
    if(aspellinfo)
    {
        AspellConfig *config;
        AspellSpeller *speller;

        memset(aspellinfo, 0, sizeof(ASPELLINFO));

        // Load the aspell library and set up our function pointers, etc.
        if(!speller_load_aspell_library(aspellinfo))
        {
            speller_close(aspellinfo);
            return NULL;
        }

        config = aspellinfo->new_aspell_config();

        if(lang)
        {
            aspellinfo->aspell_config_replace(config, "lang", lang);
        }

        aspellinfo->aspell_config_replace(config, "encoding", "utf-8");

        aspellinfo->err = aspellinfo->new_aspell_speller(config);
        aspellinfo->delete_aspell_config(config);

        if(aspellinfo->aspell_error_number(aspellinfo->err))
        {
            aspellinfo->error_message =
                aspellinfo->aspell_error_message(aspellinfo->err);
        }
        else
        {
            aspellinfo->speller = aspellinfo->to_aspell_speller(aspellinfo->err);
            aspellinfo->err = NULL;

            /*
            TODO: Log the current speller config somewhere?

            config = aspell_speller_config(speller);

            fputs("Using: ",                                      stdout);
            fputs(aspell_config_retrieve(config, "lang"),         stdout);
            fputs("-",                                            stdout);
            fputs(aspell_config_retrieve(config, "jargon"),       stdout);
            fputs("-",                                            stdout);
            fputs(aspell_config_retrieve(config, "size"),         stdout);
            fputs("-",                                            stdout);
            fputs(aspell_config_retrieve(config, "module"),       stdout);
            fputs("\n\n",                                         stdout);
             */
        }
    }

    return aspellinfo;
}

/*
 * Get the last aspell error message.
 */
const char *
speller_get_error_message(ASPELLINFO *aspellinfo)
{
    return aspellinfo ? aspellinfo->error_message : NULL;
}

/*
 * Close up shop.
 */
void
speller_close(ASPELLINFO *aspellinfo)
{
    if(aspellinfo)
    {
        // Make sure someone hasn't missed a speller_suggestion_close
        assert(!aspellinfo->suggestion_elements);

        if(aspellinfo->err)
        {
            aspellinfo->delete_aspell_can_have_error(aspellinfo->err);
            aspellinfo->err = NULL;
        }

        if(aspellinfo->speller)
        {
            aspellinfo->delete_aspell_speller(aspellinfo->speller);
            aspellinfo->speller = NULL;
        }

        if(aspellinfo->mod_aspell)
        {
            FreeLibrary(aspellinfo->mod_aspell);
            aspellinfo->mod_aspell = NULL;
        }

        free(aspellinfo);
    }
}

/*
 * Check the dictionary for a word.
 *  1:found, 0:not found, -1:error
 */
int
speller_check_word(ASPELLINFO *aspellinfo, const char *word, int word_size)
{
    int have = 1;

    if(!isdigit(*word))
    {
        have = aspellinfo->aspell_speller_check(aspellinfo->speller, word, word_size);
        if(have == -1)
        {
            aspellinfo->error_message =
                aspellinfo->aspell_speller_error_message(aspellinfo->speller);
        }

    }

    return have;
}

/*
 * Add word to dictionary.
 *  1:success, 0:failed
 */
int
speller_add_to_dictionary(ASPELLINFO *aspellinfo, const char *word, int word_size)
{
    aspellinfo->aspell_speller_add_to_personal(aspellinfo->speller,
        word, word_size);
    if(aspellinfo->aspell_speller_error(aspellinfo->speller))
    {
        aspellinfo->error_message =
            aspellinfo->aspell_speller_error_message(aspellinfo->speller);
        return 0;
    }

    aspellinfo->aspell_speller_save_all_word_lists(aspellinfo->speller);
    if(aspellinfo->aspell_speller_error(aspellinfo->speller))
    {
        aspellinfo->error_message =
            aspellinfo->aspell_speller_error_message(aspellinfo->speller);
        return 0;
    }

    return 1;
}

/*
 * Add this word to the current spelling session.
 *  1:success, 0:failed
 */
int
speller_ignore_all(ASPELLINFO *aspellinfo, const char *word, int word_size)
{
    aspellinfo->aspell_speller_add_to_session(aspellinfo->speller, word, word_size);
    if(aspellinfo->aspell_speller_error(aspellinfo->speller))
    {
        aspellinfo->error_message =
            aspellinfo->aspell_speller_error_message(aspellinfo->speller);
        return 0;
    }

    return 1;
}

/*
 * Replace a word.
 *  1:success, 0:failed
 */
int
speller_replace_word(ASPELLINFO *aspellinfo,
    const char * mis, int mis_size, const char * cor, int cor_size)
{
    int ret;

    ret = aspellinfo->aspell_speller_store_replacement(aspellinfo->speller,
            mis, mis_size, cor, cor_size);
    if(ret == -1)
    {
        aspellinfo->error_message =
            aspellinfo->aspell_speller_error_message(aspellinfo->speller);
        return 0;
    }

    return 1;
}

/*
 * Init the suggestions element array for a specific word.
 *  1:success, 0:failed
 */
int
speller_suggestion_init(ASPELLINFO *aspellinfo, const char *word, int word_size)
{
    const AspellWordList *suggestions;

    // Make sure someone hasn't missed a speller_suggestion_close
    assert(!aspellinfo->suggestion_elements);

    suggestions = aspellinfo->aspell_speller_suggest(aspellinfo->speller,
        word, word_size);
    if(!suggestions)
    {
        aspellinfo->error_message =
            aspellinfo->aspell_speller_error_message(aspellinfo->speller);
        return 0;
    }

    aspellinfo->suggestion_elements =
        aspellinfo->aspell_word_list_elements(suggestions);
    return 1;
}

/*
 * Find next suggestion. Returns NULL when no more are left.
 */
const char *
speller_suggestion_getnext(ASPELLINFO *aspellinfo)
{
    return aspellinfo->aspell_string_enumeration_next(
        aspellinfo->suggestion_elements);
}

/*
 * Close the suggestion element array.
 */
void
speller_suggestion_close(ASPELLINFO *aspellinfo)
{
    aspellinfo->delete_aspell_string_enumeration(aspellinfo->suggestion_elements);
    aspellinfo->suggestion_elements = NULL;
}
