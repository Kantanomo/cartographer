#pragma once
#include <H2MOD/Modules/SpecialEvents/SpecialEvents.h>
#include <H2MOD/Modules/Input/ControllerInput.h>
#include "H2MOD/Utils/EasyJsonStruct.cpp"

struct s_cartographer_settings : s_base_easy_json_struct<s_cartographer_settings> {
	bool h2portable = false;
	unsigned short base_port = 2000;
	bool upnp = true;
	bool enable_xdelay = true;
	bool debug_log = false;
	int debug_log_level = 2;
	bool debug_log_console = false;
	bool language_label_capture = false;
	bool discord_enable = true;
	unsigned long internal_ip = 0;
	unsigned long external_ip = 0;

	struct s_language_code {
		int code_main;
		int code_variant;
	};
	s_language_code language_code = { -1,0 };

	struct s_game_settings {
		bool skip_intro = false;
		bool melee_fix = true;
		bool no_events = false;
		bool skeleton_biped = true;

		struct s_video_settings {
			int fps_limit = 60;
			int static_lod_scale = 0;
			int field_of_view = 78;
			int vehicle_field_of_view = 78;
			bool static_fp_fov = false;
			int refresh_rate = 60;
			bool shader_lod_max = false;
			bool light_suppressor = false;
			bool d3dex = false;

			enum e_rendering_mode : byte
			{
				_rendering_mode_none,
				_rendering_mode_original_game_frame_limit
			};
			e_rendering_mode experimental_rendering = _rendering_mode_none;
			enum e_override_texture_resolution : int
			{
				tex_low,
				tex_default,
				tex_high,
				tex_ultra
			};
			e_override_texture_resolution override_shadows = tex_default;
			e_override_texture_resolution override_water = tex_default;
		};
		s_video_settings video;

		struct s_hud_settings {
			float crosshair_scale = 1.0;
			bool hide_ingame_chat = false;
			float crosshair_offset = 0.138;
		};
		s_hud_settings hud;

		struct s_input_settings {
			bool raw_mouse_input = false;
			float mouse_raw_scale = 25.0;
			bool mouse_uniform_sens = false;
			bool disable_ingame_keyboard = false;
			int hotkey_help = 113;
			int hotkey_align_window = 118;
			int hotkey_window_mode = 119;
			int hotkey_hide_ingame_chat = 120;
			int hotkey_guide = 36;
			int hotkey_console = 121;
			float controller_sens = 0.0;
			bool controller_modern = false;
			enum e_deadzone_type : byte {
				Axial,
				Radial,
				Both
			};
			e_deadzone_type deadzone_type = Axial;
			float deadzone_axial_x = 26.0;
			float deadzone_axial_y = 26.0;
			float deadzone_radial = 1.0;

			ControllerInput::CustomControllerLayout controller_layout = "1-2-4-8-16-32-64-128-256-512-4096-8192-16384-32768";
		};
		s_input_settings input;
	};
	s_game_settings game;

	struct s_server_settings {
		char server_name[XUSER_NAME_SIZE] = "A Halo 2 Server";
		char playlist[256] = { "" };
		char login_identifier[255] = { "" };
		char login_password[255] = { "" };
		int additional_pcr_time = 25;
		int minimum_player_start = 0;
		bool vip_lock = false;
		bool shuffle_even_teams = false;
		bool koth_random = true;
		char enabled_teams_bit_str[16] = "1-1-1-1-1-1-1-1";
		short enabled_team_bit_flags = 0xFF;
		bool enabled_team_flag_array[8];
		bool enable_anti_cheat = true;
	};
	s_server_settings server;

	struct s_development_settings {
		e_special_event_type forced_event = _no_event;
	};

	s_development_settings development;

	void load(easy_json_struct<s_cartographer_settings>& json) override;
	void save(easy_json_struct<s_cartographer_settings>& json) override;
};

extern s_cartographer_settings cartographer_settings;