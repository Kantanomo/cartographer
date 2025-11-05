#pragma once
#include "tag_files/data_reference.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_block.h"

/* public code */

void hud_messaging_apply_hooks(void);

void __cdecl hud_messaging_update(int32 user_index);

void __cdecl hud_messaging_clear(void);

void __cdecl hud_messaging_post(int32 user_index, string_id string_id);
