/*
 * ========================================================================
 * FILE: MSWIN_ASPELL.H
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

typedef struct ASPELLINFO ASPELLINFO;

ASPELLINFO *speller_init(char *lang);
void speller_close(ASPELLINFO *aspellinfo);

const char *speller_get_error_message(ASPELLINFO *aspellinfo);

int speller_check_word(ASPELLINFO *aspellinfo, const char *word, int word_size);
int speller_add_to_dictionary(ASPELLINFO *aspellinfo, const char *word, int word_size);
int speller_ignore_all(ASPELLINFO *aspellinfo, const char *word, int word_size);
int speller_replace_word(ASPELLINFO *aspellinfo, const char * mis, int mis_size,
    const char * cor, int cor_size);

int speller_suggestion_init(ASPELLINFO *aspellinfo, const char *word, int word_size);
const char *speller_suggestion_getnext(ASPELLINFO *aspellinfo);
void speller_suggestion_close(ASPELLINFO *aspellinfo);

