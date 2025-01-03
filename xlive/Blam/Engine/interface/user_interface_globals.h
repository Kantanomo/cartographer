#pragma once
#include "user_interface_widget.h"

#include "saved_games/player_profile.h"
#include "tag_files/tag_block.h"
#include "tag_files/tag_reference.h"

/* forward declarations */

enum e_scenario_type : int16;

/* structures */

struct s_user_interface_tag_globals
{
	// Explaination("Shared Globals", "This is a reference to the ui shared globals tag")
	tag_reference shared_globals;	// wigl
	
	// Explaination("Screen Widgets", "These are the screen widgets")
	tag_block<s_user_interface_widget_reference> screen_widgets;

	// Explaination("Multiplayer Variant Settings Interface", "This blob defines the ui for setting multiplayer game variant parameters")
	tag_reference mp_variant_settings_ui;	// goof
	
	// Explaination("Game Hopper Localization Strings", "This is for the loc game hopper strings")
	tag_reference game_hopper_descriptions;	// unic
};
ASSERT_STRUCT_SIZE(s_user_interface_tag_globals, 32);

/* prototypes */

/* gets the current loaded user_interface_globals tag aka wgtz depending on the scenario type */
static s_user_interface_tag_globals* _cdecl user_interface_get_tag_globals(void)
{
	// dives into globals/globals tag and gets the block "Interface Tags"
	// from there checks current scnr type and itself returns a pointer for the correct one
	return INVOKE(0x20BB1D, 0x1F2C5A, user_interface_get_tag_globals);
}

bool __cdecl user_interface_globals_is_beta_build();
int32 __cdecl user_interface_globals_get_game_difficulty();
int32 __cdecl user_interface_globals_get_edit_player_profile_index();
s_saved_game_player_profile* user_interface_globals_get_edit_player_profile();
e_scenario_type __cdecl user_interface_globals_get_map_type();
void __cdecl user_interface_globals_set_game_difficulty_real(int32 difficulty);
void __cdecl user_interface_globals_set_loading_from_persistent_storage(bool a1);
void __cdecl user_interface_globals_commit_edit_profile_changes();
void __cdecl user_interface_globals_save_profile_changes_to_disk();
void __cdecl user_interface_globals_finish_saving_profile_changes();
void __cdecl user_interface_globals_set_edit_player_profile(e_controller_index controller_index, uint32 profile_index, s_saved_game_player_profile* profile);
void __cdecl user_interface_globals_save_edit_profile_to_disk();
