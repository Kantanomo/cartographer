#pragma once
#include "definitions/halo_playlist_header.h"
#include "main/map_manager.h"
#include "saved_games/game_variant.h"

/* constants */

enum
{
	k_halo_playlist_maximum_variant_count = 100,
	k_halo_playlist_maximum_match_count = 100,
	k_halo_playlist_maximum_errors = 200,
	k_halo_playlist_default_delay_time = 10,
	k_halo_playlist_maximum_file_sections = 112,
	k_halo_playlist_maximum_text_length = 32
};

/* enums */

enum e_halo_playlist_loading_result
{
	_halo_playlist_loading_result_success,
	_halo_playlist_loading_result_file_not_found,
	_halo_playlist_loading_result_invalid_data,
	_halo_playlist_loading_result_unexpected_end_of_file,
	_halo_playlist_loading_result_empty_playlist
};

enum e_halo_playlist_error
{
	_halo_playlist_error_playlist_header_already_defined,
	_halo_playlist_error_duplicate_variant_found,
	_halo_playlist_error_property_name_invalid,
	_halo_playlist_error_header_name_invalid,
	_halo_playlist_error_property_value_invalid,
	_halo_playlist_error_property_already_defined,
	_halo_playlist_error_match_property_invalid,
	_halo_playlist_error_variant_setting_invalid,
	_halo_playlist_error_variant_base_or_game_type_not_found,
	_halo_playlist_error_variant_name_not_found,
	_halo_playlist_error_variant_name_invalid,
	_halo_playlist_error_variant_name_illegal_character,
	_halo_playlist_error_variant_invalid,
	_halo_playlist_error_variant_base_variant_and_game_type_both_set,
	_halo_playlist_error_match_variant_not_found,
	_halo_playlist_error_match_map_not_found,
	_halo_playlist_error_max_variants,
	_halo_playlist_error_max_matches,
};

enum e_halo_playlist_reader_seek_mode
{
	_halo_playlist_reader_seek_mode_new_line,
	_halo_playlist_reader_seek_mode_unused,
	_halo_playlist_reader_seek_mode_header_start,
	_halo_playlist_reader_seek_mode_header_read,
	_halo_playlist_reader_seek_mode_property_name_read,
	_halo_playlist_reader_seek_mode_property_deliminator_scan,
	_halo_playlist_reader_seek_mode_property_value_read,
	_halo_playlist_reader_seek_mode_seek_to_next_line,
};

/* structures */

struct s_halo_playlist_section_item
{
	wchar_t name_buffer[k_halo_playlist_maximum_text_length];
	wchar_t value_buffer[k_halo_playlist_maximum_text_length];
	uint32 file_line;
	bool processed;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_section_item, 0x88);

struct s_halo_playlist_match
{
	uint32 match_line_in_file;
	wchar_t map[k_halo_playlist_maximum_text_length];
	uint32 map_line_in_file;
	wchar_t variant[17];
	uint32 variant_line_in_file;
	uint32 weight;
	uint16 minimum_players;
	uint16 maximum_players;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_match, 0x78);

struct s_halo_playlist_match_variant
{
	s_game_variant variant;
	int32 map_id;
	s_secure_map_id custom_map_id;
	char map_source_path[128];
	uint32 weight;
	uint16 minimum_players;
	uint16 maximum_players;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_match_variant, 0x21C);

struct s_halo_playlist_error
{
	e_halo_playlist_error error_type;
	uint32 file_line;
	wchar_t property_name[k_halo_playlist_maximum_text_length];
	wchar_t property_value[k_halo_playlist_maximum_text_length];
	wchar_t property_extra[k_halo_playlist_maximum_text_length];
};
ASSERT_STRUCT_SIZE(s_halo_playlist_error, 0xC8);

struct s_halo_playlist
{
	bool shuffle;
	uint32 pregame_team_selection_delay;
	uint32 pregame_delay;
	uint32 postgame_delay;
	s_halo_playlist_match_variant match_variants[k_halo_playlist_maximum_match_count];
	uint32 match_variant_count;
	s_game_variant variants[k_halo_playlist_maximum_variant_count];
	uint32 variant_count;
	s_halo_playlist_error errors[k_halo_playlist_maximum_errors];
	uint32 error_log_count;
	bool error_log_full;
};
ASSERT_STRUCT_SIZE(s_halo_playlist, 0x1E610);

struct s_halo_playlist_loading_result
{
	int32 result_code;
	wchar_t path[MAX_PATH];
};
ASSERT_STRUCT_SIZE(s_halo_playlist_loading_result, 524);

struct s_halo_playlist_container
{
	s_halo_playlist playlist;
	s_halo_playlist_loading_result result;
};
ASSERT_STRUCT_SIZE(s_halo_playlist_container, 0x1E81C);

class c_halo_playlist_reader
{
public:
	s_halo_playlist* m_playlist;
	s_halo_playlist_match m_matches[k_halo_playlist_maximum_match_count];
	uint32 m_match_count;
	s_halo_playlist_section_item m_section_items[k_halo_playlist_maximum_file_sections];
	uint32 m_section_buffer_item_count;
	wchar_t m_section_header_buffer[k_halo_playlist_maximum_text_length];
	DWORD m_current_header_file_line;
	e_halo_playlist_header_type m_current_section_type;
	e_halo_playlist_reader_seek_mode m_current_reader_mode;
	uint32 m_current_reader_char_index;
	uint32 m_current_reader_line;
	bool m_playlist_header_found;
	bool unk;
	
	void parse_file_section(const wchar_t* file_buffer);
	void evaluate_current_header();
	void process_current_header();

	void process_variant_section();
	bool process_variant_match_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_player_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_team_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_vehicle_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_equipment_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_cartographer_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_slayer_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_oddball_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_juggernaut_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_king_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_ctf_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_assault_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);
	bool process_variant_territories_setting(s_halo_playlist_section_item* section_item, s_game_variant* variant);

	void process_match_section();
	void process_current_property();
	void process_playlist_property(s_halo_playlist_section_item* section);
	void process_variant_property(s_halo_playlist_section_item* section);
	void process_match_property(s_halo_playlist_section_item* section);
	bool property_name_is_valid(wchar_t* property_name);
	void trim_property_name();
	void log_error(e_halo_playlist_error error_type, uint32 file_line, wchar_t* property_name = L"", wchar_t* property_value = L"", wchar_t* extra = L"");
	void finalize();

	static int game_variant_sort(void* context, void const* lhs, void const* rhs);
	static int game_variant_search(void* context, void const* lhs, void const* rhs);
	static int playlist_error_sort(void* context, void const* lhs, void const* rhs);
};

ASSERT_STRUCT_SIZE(c_halo_playlist_reader, 0x6AC4);

/* prototypes */

void halo_playlist_apply_patches();