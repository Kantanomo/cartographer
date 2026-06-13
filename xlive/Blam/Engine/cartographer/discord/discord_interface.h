#pragma once
#include "networking/panorama/panorama_presence.h"

/* macros */

// #define TEST_DISCORD_INSTANCE

/* prototypes */

// Initialize discord interface
void discord_game_status_create(HMODULE module);

// Deinitialize discord interface
void discord_game_status_dispose(void);

// Set small image for discord interface
void discord_interface_set_small_image(const char* small_image, const char* small_text);

// Set large image for discord interface
void discord_interface_set_large_image(const char* large_image, const char* large_text);

// Set details for discord interface
void discord_interface_set_details(const char* details);

// Set state for discord interface
void discord_interface_set_state(const char* state);

// Update map info for discord interface
void discord_interface_set_map_name(const utf8* scenario_name, const utf8* map_name);

// Update variant info for discord interface
void discord_interface_set_variant(e_context_variant variant, const utf8* variant_name);

// Update difficulty info for discord interface
void discord_interface_set_difficulty(int16 difficulty);

// Sets player counts to 0 and removes player info text from status
void discord_interface_zero_player_count(void);

// Update player count for discord interface
void discord_interface_set_player_counts(void);

void discord_interface_update_map_info_campaign(int32 map_id);

void discord_interface_set_context(e_context_id context_id, uint32 contex_value);
