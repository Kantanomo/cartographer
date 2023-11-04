#include "stdafx.h"
#include "settings.h"
#include <H2MOD/Modules/Shell/Startup/Startup.h>



void c_cartographer_settings::load(c_easy_json<c_cartographer_settings>& json)
{
	const auto settings = this;
	json["cartographer"].get_ds("h2portable", &settings->h2portable);
	json["cartographer"].get_ds("base_port", &settings->base_port);
	json["cartographer"].get_ds("upnp", &settings->upnp);
	json["cartographer"].get_ds("enable_xdelay", &settings->enable_xdelay);
	json["cartographer"].get_ds("debug_log", &settings->debug_log);
	json["cartographer"].get_ds("debug_log_level", &settings->debug_log_level);
	json["cartographer"].get_ds("debug_log_console", &settings->debug_log_console);
	json["cartographer"].get_ds("language_label_capture", &settings->language_label_capture);
	json["cartographer"].get_ds("discord_enable", &settings->discord_enable);

	const char* wan_ip_string = json["cartographer"].get<const char*>("wan_ip", "");
	if (strnlen_s(wan_ip_string, 15) >= 7 && inet_addr(wan_ip_string) != INADDR_NONE)
	{
		strncpy(settings->wan_ip_str, wan_ip_string, 15);
		settings->wan_ip = inet_addr(settings->wan_ip_str);
	}

	const char* lan_ip_string = json["cartographer"].get<const char*>("lan_ip", "");
	if (strnlen_s(lan_ip_string, 15) >= 7 && inet_addr(lan_ip_string) != INADDR_NONE)
	{
		strncpy(settings->lan_ip_str, lan_ip_string, 15);
		settings->lan_ip = inet_addr(settings->lan_ip_str);
	}

	if (auto language_code = json["cartographer"].get<std::string>("language_code", "-1x0"); !language_code.empty())
	{
		auto delim_offset = language_code.find("x");
		if (delim_offset != std::string::npos)
		{
			std::string code_main_substr = language_code.substr(0, delim_offset);
			std::string code_variant_substr = language_code.substr(delim_offset + 1, language_code.size());
			settings->language_code.code_main = stol(code_main_substr);
			settings->language_code.code_variant = stol(code_variant_substr);
		}
	}
	
	if (!H2IsDediServer)
	{
		json["game"].get_ds("skip_intro", &settings->game.skip_intro);
		json["game"].get_ds("melee_fix", &settings->game.melee_fix);
		json["game"].get_ds("no_events", &settings->game.no_events);
		json["game"].get_ds("skeleton_biped", &settings->game.skeleton_biped);

		json["game"]["video"].get_ds("fps_limit", &settings->game.video.fps_limit);
		//json["game"]["video"].get_ds("static_lod_scale", &settings->game.video.static_lod_scale);
		switch (json["game"]["video"].get<int>("static_lod_scale", 0))
		{
		default:
		case 0:
			settings->game.video.static_lod_scale = lod_disable;
			break;
		case 1:
			settings->game.video.static_lod_scale = lod_super_low;
			break;
		case 2:
			settings->game.video.static_lod_scale = lod_low;
			break;
		case 3:
			settings->game.video.static_lod_scale = lod_medium;
			break;
		case 4:
			settings->game.video.static_lod_scale = lod_high;
			break;
		case 5:
			settings->game.video.static_lod_scale = lod_super_high;
			break;
		case 6:
			settings->game.video.static_lod_scale = lod_cinematic;
			break;
		}
		json["game"]["video"].get_ds("field_of_view", &settings->game.video.field_of_view);
		json["game"]["video"].get_ds("vehicle_field_of_view", &settings->game.video.vehicle_field_of_view);
		json["game"]["video"].get_ds("static_fp_fov", &settings->game.video.static_fp_fov);

		switch (json["game"]["video"].get<int>("experimental_rendering", 0))
		{
		default:
			//Incase any of the old rendering modes were used for a higher fps, set it back to 60.
			settings->game.video.fps_limit = 60;
		case 0:
			settings->game.video.experimental_rendering = e_frame_limiter_type::_rendering_mode_none;
			break;
		case 1:
			settings->game.video.experimental_rendering = e_frame_limiter_type::_rendering_mode_original_game_frame_limit;
			break;
		}

		json["game"]["video"].get_ds("refresh_rate", &settings->game.video.refresh_rate);
		json["game"]["video"].get_ds("shader_lod_max", &settings->game.video.shader_lod_max);
		json["game"]["video"].get_ds("light_suppressor", &settings->game.video.light_suppressor);
		json["game"]["video"].get_ds("d3dex", &settings->game.video.d3dex);

		switch (json["game"]["video"].get<int>("override_shadows", 1))
		{
		case 0:
			settings->game.video.override_shadows = e_override_texture_resolution::tex_low;
			break;
		default:
		case 1:
			settings->game.video.override_shadows = e_override_texture_resolution::tex_default;
			break;
		case 2:
			settings->game.video.override_shadows = e_override_texture_resolution::tex_high;
			break;
		case 3:
			settings->game.video.override_shadows = e_override_texture_resolution::tex_ultra;
			break;
		}
		switch (json["game"]["video"].get<int>("override_water", 1))
		{
		case 0:
			settings->game.video.override_water = e_override_texture_resolution::tex_low;
			break;
		default:
		case 1:
			settings->game.video.override_water = e_override_texture_resolution::tex_default;
			break;
		case 2:
			settings->game.video.override_water = e_override_texture_resolution::tex_high;
			break;
		case 3:
			settings->game.video.override_water = e_override_texture_resolution::tex_ultra;
			break;
		}

		json["game"]["hud"].get_ds("crosshair_offset", &settings->game.hud.crosshair_offset);
		json["game"]["hud"].get_ds("crosshair_scale", &settings->game.hud.crosshair_scale);
		json["game"]["hud"].get_ds("hide_ingame_chat", &settings->game.hud.hide_ingame_chat);

		json["game"]["input"].get_ds("raw_mouse_input", &settings->game.input.raw_mouse_input);
		json["game"]["input"].get_ds("mouse_raw_scale", &settings->game.input.mouse_raw_scale);
		json["game"]["input"].get_ds("mouse_uniform_sens", &settings->game.input.mouse_uniform_sens);
		json["game"]["input"].get_ds("mouse_sensitivity", &settings->game.input.mouse_sensitivity);
		json["game"]["input"].get_ds("disable_ingame_keyboard", &settings->game.input.disable_ingame_keyboard);
		json["game"]["input"].get_ds("hotkey_help", &settings->game.input.hotkey_help);
		json["game"]["input"].get_ds("hotkey_align_window", &settings->game.input.hotkey_align_window);
		json["game"]["input"].get_ds("hotkey_window_mode", &settings->game.input.hotkey_window_mode);
		json["game"]["input"].get_ds("hotkey_hide_ingame_chat", &settings->game.input.hotkey_hide_ingame_chat);
		json["game"]["input"].get_ds("hotkey_guide", &settings->game.input.hotkey_guide);
		json["game"]["input"].get_ds("hotkey_console", &settings->game.input.hotkey_console);
		json["game"]["input"].get_ds("controller_sens", &settings->game.input.controller_sens);
		json["game"]["input"].get_ds("controller_modern", &settings->game.input.controller_modern);

		switch (json["game"]["input"].get<int>("deadzone_type", 0))
		{
		default:
		case 0:
			settings->game.input.deadzone_type = e_controller_deadzone_type::axial_deadzone;
			break;
		case 1:
			settings->game.input.deadzone_type = e_controller_deadzone_type::radial_deadzone;
			break;
		case 2:
			settings->game.input.deadzone_type = e_controller_deadzone_type::both_deadzone;
			break;
		}

		json["game"]["input"].get_ds("deadzone_axial_x", &settings->game.input.deadzone_axial_x);
		json["game"]["input"].get_ds("deadzone_axial_y", &settings->game.input.deadzone_axial_y);
		json["game"]["input"].get_ds("deadzone_radial", &settings->game.input.deadzone_radial);
		settings->game.input.controller_layout = json["game"]["input"].get<const char*>("controller_layout", "1-2-4-8-16-32-64-128-256-512-4096-8192-16384-32768");
	}
	if (H2IsDediServer)
	{
		const auto server_name = json["server"].get<const char*>("server_name", "Halo 2 Server");
		if (server_name)
			strncpy(settings->server.server_name, server_name, XUSER_MAX_NAME_LENGTH);
		const auto server_playlist = json["server"].get<const char*>("server_playlist", "");
		if (server_playlist)
			strncpy(settings->server.playlist, server_playlist, sizeof(settings->server.playlist));
		const auto login_identifier = json["server"].get<const char*>("login_identifier", "");
		if (login_identifier)
			strncpy(settings->server.login_identifier, login_identifier, sizeof(settings->server.login_identifier));
		const auto login_password = json["server"].get<const char*>("login_password", "");
		if (login_password)
			strncpy(settings->server.login_password, login_password, sizeof(settings->server.login_password));

		json["server"].get_ds("additional_pcr_time", &settings->server.additional_pcr_time);
		json["server"].get_ds("minimum_player_start", &settings->server.minimum_player_start);
		json["server"].get_ds("vip_lock", &settings->server.vip_lock);
		json["server"].get_ds("shuffle_even_teams", &settings->server.shuffle_even_teams);
		json["server"].get_ds("enable_anti_cheat", &settings->server.enable_anti_cheat);

		auto team_bit_mask = json["server"].get<std::string>("teams_enabled_bit_flags", settings->server.enabled_teams_bit_str);
		if (!team_bit_mask.empty())
		{
			strncpy_s(settings->server.enabled_teams_bit_str, sizeof(settings->server.enabled_teams_bit_str), team_bit_mask.c_str(), 15);
			settings->server.enabled_team_bit_flags = 0;
			memset(settings->server.enabled_team_flag_array, 0, sizeof(settings->server.enabled_team_flag_array));

			const size_t true_bit_value_count = std::count(team_bit_mask.begin(), team_bit_mask.end(), '1');
			const size_t false_bit_value_count = std::count(team_bit_mask.begin(), team_bit_mask.end(), '0');

			const char team_bit_to_find[] = "01";
			size_t occurance_offset;
			occurance_offset = team_bit_mask.find_first_of(team_bit_to_find, 0);

			// TODO move to function
			// validate first
			if (true_bit_value_count + false_bit_value_count == 8
				&& occurance_offset != std::string::npos)
			{
				// then loop
				for (int i = 0; i < 8; i++)
				{
					if (team_bit_mask.at(occurance_offset) == '1') // check if the team is enabled
					{
						settings->server.enabled_team_bit_flags |= FLAG(i); // if so, enable the flag
						settings->server.enabled_team_flag_array[i] = true;
					}

					occurance_offset = team_bit_mask.find_first_of(team_bit_to_find, occurance_offset + 1);
					if (occurance_offset == std::string::npos)
						break;
				}
			}
		}
	}
#ifndef NDEBUG
	settings->development.forced_event = (e_special_event_type)json["development"].get("forced_event", 0);
#endif
}

void c_cartographer_settings::save(c_easy_json<c_cartographer_settings>& json)
{
	const auto settings = this;
	json["cartographer"].set("h2portable", settings->h2portable);
	json["cartographer"].set("base_port", settings->base_port);
	json["cartographer"].set("upnp", settings->upnp);
	json["cartographer"].set("enable_xdelay", settings->enable_xdelay);
	json["cartographer"].set("debug_log", settings->debug_log);
	json["cartographer"].set("debug_log_level", settings->debug_log_level);
	json["cartographer"].set("debug_log_console", settings->debug_log_console);
	json["cartographer"].set("language_label_capture", settings->language_label_capture);
	json["cartographer"].set("discord_enable", settings->discord_enable);
	json["cartographer"].set("wan_ip", settings->wan_ip_str);
	json["cartographer"].set("lan_ip", settings->wan_ip_str);
	std::string lang_str(std::to_string(settings->language_code.code_main) + "x" + std::to_string(settings->language_code.code_variant));
	json["cartographer"].set("language_code", lang_str);

	if (!H2IsDediServer) {
		json["game"].set("skip_intro", settings->game.skip_intro);
		json["game"].set("melee_fix", settings->game.melee_fix);
		json["game"].set("no_events", settings->game.no_events);
		json["game"].set("skeleton_biped", settings->game.skeleton_biped);

		json["game"]["video"].set("fps_limit", settings->game.video.fps_limit);
		json["game"]["video"].set("static_lod_scale", (int)settings->game.video.static_lod_scale);
		json["game"]["video"].set("field_of_view", settings->game.video.field_of_view);
		json["game"]["video"].set("vehicle_field_of_view", settings->game.video.vehicle_field_of_view);
		json["game"]["video"].set("static_fp_fov", settings->game.video.static_fp_fov);
		json["game"]["video"].set("experimental_rendering", (int)settings->game.video.experimental_rendering);
		json["game"]["video"].set("refresh_rate", settings->game.video.refresh_rate);
		json["game"]["video"].set("shader_lod_max", settings->game.video.shader_lod_max);
		json["game"]["video"].set("light_suppressor", settings->game.video.light_suppressor);
		json["game"]["video"].set("d3dex", settings->game.video.d3dex);
		json["game"]["video"].set("override_shadows", (int)settings->game.video.override_shadows);
		json["game"]["video"].set("override_water", (int)settings->game.video.override_water);

		json["game"]["hud"].set("crosshair_offset", settings->game.hud.crosshair_offset);
		json["game"]["hud"].set("crosshair_scale", settings->game.hud.crosshair_scale);
		json["game"]["hud"].set("hide_ingame_chat", settings->game.hud.hide_ingame_chat);

		json["game"]["input"].set("raw_mouse_input", settings->game.input.raw_mouse_input);
		json["game"]["input"].set("mouse_raw_scale", settings->game.input.mouse_raw_scale);
		json["game"]["input"].set("mouse_uniform_sens", settings->game.input.mouse_uniform_sens);
		json["game"]["input"].set("mouse_sensitivity", settings->game.input.mouse_sensitivity);
		json["game"]["input"].set("disable_ingame_keyboard", settings->game.input.disable_ingame_keyboard);
		json["game"]["input"].set("hotkey_help", settings->game.input.hotkey_help);
		json["game"]["input"].set("hotkey_align_window", settings->game.input.hotkey_align_window);
		json["game"]["input"].set("hotkey_window_mode", settings->game.input.hotkey_window_mode);
		json["game"]["input"].set("hotkey_hide_ingame_chat", settings->game.input.hotkey_hide_ingame_chat);
		json["game"]["input"].set("hotkey_guide", settings->game.input.hotkey_guide);
		json["game"]["input"].set("hotkey_console", settings->game.input.hotkey_console);
		json["game"]["input"].set("controller_sens", settings->game.input.controller_sens);
		json["game"]["input"].set("controller_modern", settings->game.input.controller_modern);
		json["game"]["input"].set("deadzone_type", (int)settings->game.input.deadzone_type);
		json["game"]["input"].set("deadzone_axial_x", settings->game.input.deadzone_axial_x);
		json["game"]["input"].set("deadzone_axial_y", settings->game.input.deadzone_axial_y);
		json["game"]["input"].set("deadzone_radial", settings->game.input.deadzone_radial);
		json["game"]["input"].set("controller_layout", settings->game.input.controller_layout.ToString());
	}
	if (H2IsDediServer)
	{
		json["server"].set("server_name", settings->server.server_name);
		json["server"].set("server_playlist", settings->server.playlist);
		json["server"].set("login_identifier", settings->server.login_identifier);
		json["server"].set("login_password", settings->server.login_password);
		json["server"].set("additional_pcr_time", settings->server.additional_pcr_time);
		json["server"].set("minimum_player_start", settings->server.minimum_player_start);
		json["server"].set("vip_lock", settings->server.vip_lock);
		json["server"].set("shuffle_even_teams", settings->server.shuffle_even_teams);
		json["server"].set("enable_anti_cheat", settings->server.enable_anti_cheat);
		json["server"].set("teams_enabled_bit_flags", settings->server.enabled_teams_bit_str);
	}
#ifndef NDEBUG
	json["development"].set("forced_event", (int)settings->development.forced_event);
#endif 

}

c_cartographer_settings cartographer_settings;