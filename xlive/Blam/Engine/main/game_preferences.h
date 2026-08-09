#pragma once

#include "cseries/language.h"

/* prototypes */

e_language get_current_language(void);

void __cdecl global_preferences_initialize(void);

void __cdecl global_preferences_update(void);

void __cdecl global_preferences_flag_dirty(void);

int32 __cdecl language_get_international_key(e_language lang, int32 value);