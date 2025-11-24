#pragma once

/* prototypes */

void game_state_call_before_save_procs(uint32 game_state_proc_flags);
void game_state_call_after_save_procs(uint32 game_state_proc_flags);

void game_state_procs_apply_patches(void);