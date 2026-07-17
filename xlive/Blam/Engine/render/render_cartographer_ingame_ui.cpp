#include "stdafx.h"
#include "render_cartographer_ingame_ui.h"

#include "render.h"
#include "cartographer/build_version/cartographer_build_version.h"
#include "cartographer/twizzler/twizzler.h"

#include "game/game.h"
#include "game/game_engine.h"
#include "rasterizer/rasterizer_globals.h"
#include "rasterizer/rasterizer_text.h"
#include "rasterizer/dx9/rasterizer_dx9.h"
#include "shell/shell_windows.h"
#include "networking/logic/life_cycle_manager.h"
#include "networking/session/network_observer.h"
#include "networking/session/network_session.h"
#include "text/draw_string.h"
#include "text/font_cache.h"
#include "text/unicode.h"

#include "H2MOD/GUI/imgui_integration/imgui_handler.h"
#include "H2MOD/Modules/Accounts/AccountLogin.h"
#include "H2MOD/Modules/Achievements/Achievements.h"
#include "H2MOD/Modules/Updater/Updater.h"
#include "version_git.h"
#include "cache/cache_files.h"
#include "input/input_windows.h"
#include "interface/multiplayer_variant_settings_interface_definition.h"
#include "rasterizer/dx9/rasterizer_dx9_dof.h"
#include "rasterizer/dx9/rasterizer_dx9_primitives.h"
#include "rasterizer/dx9/rasterizer_dx9_submit.h"
#include "text/text.h"

/* defines */

// define this to enable queueing a test message in render_cartographer_achievement_message
//#define ACHIVEMENT_RENDER_DEBUG_ENABLED

// define this to render git branch information (if GEN_GIT_VER_VERSION_STRING is defined)
#if RELEASE_DLL == false
#define CARTOGRAPHER_TEST_BUILD_DRAW_TEXT
#endif

/* constants */

enum
{
	k_status_text_font = 0,
	k_update_status_font = 5,
	k_cheevo_title_font = 10,
	k_cheevo_message_font = 1,
	k_netdebug_text_font = 0,
	k_cheevo_display_lifetime = (5 * k_shell_time_msec_denominator),
	k_text_drawing_padding = 5
};


/* prototypes */

static void render_cartographer_status_bar(const char* build_text);
static void render_cartographer_git_build_info(void);
static bool render_cartographer_achievement_message(const char* achivement_message);
static void render_cartographer_update_message(const char* update_text, int64 update_size_bytes, int64 update_downloaded_bytes);
void render_cartographer_variant_settings();
static void render_netdebug_text(void);
static void render_main_game_time_debug(void);


/* globals */

/* public code */

void render_cartographer_ingame_ui(void)
{
	// these are global variables defined in legacy files where
	// various d3dx9 rendering functions originally were.

	// defined in Modules\Updater\Updater.cpp
#ifdef ACHIVEMENT_RENDER_DEBUG_ENABLED
	static bool x_test = false;
	if (x_test)
	{
		AchievementMap.insert({ "achievement title|achievement message", false });
		x_test = false;
	}
#endif

	rasterizer_dx9_perf_event_begin("render cartographer ingame ui", NULL);
	render_cartographer_status_bar(k_cartographer_build_text);
	render_cartographer_update_message(g_auto_update_text, sizeOfDownload, sizeOfDownloaded);
	if (!AchievementMap.empty())
	{
		auto it = AchievementMap.begin();
		it->second = true;
		if (!render_cartographer_achievement_message(it->first.c_str()))
		{
			AchievementMap.erase(it);
		}
	}
	render_cartographer_git_build_info();
	render_netdebug_text();
	render_main_game_time_debug();

	render_cartographer_variant_settings();

	rasterizer_dx9_perf_event_end("render cartographer ingame ui");
	return;
}

/* private code */

void render_cartographer_status_bar(const char *build_text)
{
	rectangle2d bounds;
	rasterizer_get_frame_bounds(&bounds);

	bool game_is_main_menu = game_is_ui_shell();
	bool paused_or_in_menus = (*Memory::GetAddress<bool*>(0x47A568) != false);

	wchar_t build_string_buffer[256];
	utf8_string_to_wchar_string(build_text, build_string_buffer, NUMBEROF(build_string_buffer));

	wchar_t master_state_string_buffer[256];
	utf8_string_to_wchar_string(GetMasterStateStr(), master_state_string_buffer, NUMBEROF(master_state_string_buffer));

	real_argb_color text_color_console = *global_real_argb_white;
	if (game_is_main_menu)
	{
		text_color_console.alpha = .5f;
	}

	const int16 line_height = get_text_size_from_font_cache(k_status_text_font);
	bool paused_or_in_main_menu = game_is_main_menu || paused_or_in_menus;
	if (paused_or_in_main_menu)
	{
		draw_string_reset();
		draw_string_set_draw_mode(k_status_text_font, 0, 0, 0, &text_color_console, global_real_argb_black, false);

		bounds.bottom = bounds.top + line_height;
		if (g_twizzler_status)
		{
			draw_string_set_format(0, 1, 0, false);
			rasterizer_draw_unicode_string(&bounds, L"Anti-cheat is enabled");
			draw_string_set_format(0, 0, 0, false);
		}
		rasterizer_draw_unicode_string(&bounds, build_string_buffer);
		bounds.top += line_height;
		bounds.bottom = bounds.top + line_height;
		rasterizer_draw_unicode_string(&bounds, master_state_string_buffer);
	}

	return;
}

void render_cartographer_git_build_info(void)
{
#if defined(GEN_GIT_VER_VERSION_STRING) && defined(CARTOGRAPHER_TEST_BUILD_DRAW_TEXT) 
	const s_rasterizer_globals* rasterizer_globals = rasterizer_globals_get();

	const int16 line_height = get_text_size_from_font_cache(k_status_text_font);
	real_argb_color text_color_console = *global_real_argb_white;
	text_color_console.alpha = .55f;

	rectangle2d bounds;
	rasterizer_get_frame_bounds(&bounds);
	bounds.top += (int16)(1050 * rasterizer_globals->ui_scale);
	bounds.left = bounds.right - (int16)(765 * rasterizer_globals->ui_scale);
	bounds.bottom = bounds.top + line_height;

	draw_string_reset();
	draw_string_set_draw_mode(k_status_text_font, 0, 0, 0, &text_color_console, global_real_argb_black, false);

	wchar_t result_text_buffer[1024];

	swprintf(result_text_buffer, NUMBEROF(result_text_buffer), L"%S %S", __DATE__, __TIME__);
	rasterizer_draw_unicode_string(&bounds, result_text_buffer);
	bounds.top += line_height;
	bounds.bottom = bounds.top + line_height;
	swprintf(result_text_buffer, NUMBEROF(result_text_buffer), L"%S %S branch: %S", GEN_GIT_VER_VERSION_STRING, GET_GIT_VER_USERNAME, GET_GIT_VER_BRANCH);
	rasterizer_draw_unicode_string(&bounds, result_text_buffer);
#endif
}

bool render_cartographer_achievement_message(const char *achivement_message)
{
	static int64 x_cheevo_timer = 0;
	int64 time_now = shell_time_now_msec();
	bool result = true;

	if (x_cheevo_timer == 0)
	{
		x_cheevo_timer = time_now + k_cheevo_display_lifetime;
	}

	if (x_cheevo_timer - time_now > 0)
	{
		rectangle2d bounds;
		wchar_t cheevo_message[256];
		const int16 widget_total_height = get_text_size_from_font_cache(k_cheevo_title_font) + (get_text_size_from_font_cache(k_cheevo_message_font) * 2);
		real_argb_color text_color = *global_real_argb_white;
		text_color.alpha = (float)(x_cheevo_timer - time_now) / (real32)k_cheevo_display_lifetime;

		utf8_string_to_wchar_string(achivement_message, cheevo_message, NUMBEROF(cheevo_message));
		wchar_t* divider_position = wcschr(cheevo_message, '|');
		if (divider_position)
		{
			*divider_position = '\0';
		}

		draw_string_reset();
		draw_string_set_draw_mode(k_cheevo_title_font, 0, 2, 0, &text_color, global_real_argb_black, false);

		rasterizer_get_screen_bounds(&bounds);
		bounds.top += rectangle2d_height(&bounds) / 3 - (widget_total_height / 2);
		rasterizer_draw_unicode_string(&bounds, L"Achievement Unlocked");
		bounds.top += get_text_size_from_font_cache(k_cheevo_title_font);

		draw_string_set_font(k_cheevo_message_font);
		rasterizer_draw_unicode_string(&bounds, cheevo_message);
		bounds.top += get_text_size_from_font_cache(k_cheevo_message_font);
		rasterizer_draw_unicode_string(&bounds, divider_position ? (divider_position + 1) : L"<invalid achievement description>");
	}
	else
	{
		x_cheevo_timer = 0;
		result = false;
	}

	return result;
}

void render_cartographer_update_message(const char* update_text, int64 update_size_bytes, int64 update_downloaded_bytes)
{
	rectangle2d bounds;
	rasterizer_get_frame_bounds(&bounds);
	bounds.top += (int16)(rectangle2d_height(&bounds) * .1f);

	if (update_text != nullptr)
	{
		wchar_t update_message_buffer[256];
		wchar_t* last_line = update_message_buffer;
		c_static_wchar_string<128> lines[16];
		int32 line_count = 0;
		int32 update_message_length = 0;

		utf8_string_to_wchar_string(update_text, update_message_buffer, NUMBEROF(update_message_buffer));
		update_message_length = ustrnlen(update_message_buffer, NUMBEROF(update_message_buffer));

		for (int32 character_index = 0; character_index < NUMBEROF(update_message_buffer) && character_index < update_message_length && line_count < NUMBEROF(lines); character_index++)
		{
			wchar_t* character = &update_message_buffer[character_index];
			if (*character == '\n')
			{
				*character = '\0';
				lines[line_count++].set(last_line);
				last_line = &update_message_buffer[character_index + 1];
			}
		}

		draw_string_reset();
		draw_string_set_draw_mode(k_update_status_font, 0, 0, 0, global_real_argb_white, global_real_argb_black, false);

		for (int32 line_index = 0; line_index < line_count; line_index++)
		{
			rasterizer_draw_unicode_string(&bounds, lines[line_index].get_string());
			bounds.top += get_text_size_from_font_cache(k_update_status_font);
		}
	}

	if (update_size_bytes > 0)
	{
		wchar_t update_message_buffer[256];
		real32 percent_complete = 100.f * ((real32)update_downloaded_bytes / update_size_bytes);
		swprintf_s(update_message_buffer, NUMBEROF(update_message_buffer), L"(progress: %.2f%%)", percent_complete);
		rasterizer_draw_unicode_string(&bounds, update_message_buffer);
	}

	return;
}

void render_cartographer_variant_settings()
{
	if (game_is_multiplayer())
	{
		key_stroke key;

		input_peek_key(&key);

		if (!input_windows_key_pressed(VK_F3))
			return;

		s_game_variant* variant = get_game_variant();

		if (!variant)
			return;

		wchar_t string_buffer[256];

		pixel32 header_color = PIXEL32_ARGB(128, 0, 0, 0);
		pixel32 body_color = PIXEL32_ARGB(64, 0, 0, 0);

		const int16 header_line_height = get_text_size_from_font_cache(k_cheevo_title_font);
		const int16 body_line_height = get_text_size_from_font_cache(k_status_text_font);

		rectangle2d frame_bounds;

		rasterizer_get_frame_bounds(&frame_bounds);

		// maybe add safe area respecting padding?
		const rectangle2d header_rect = {
			75,
			20,
			(int16)(80 + header_line_height),
			(int16)(frame_bounds.x1 - 20)
		};
		const rectangle2d body_rect =
		{
			header_rect.bottom,
			header_rect.left,
			(int16)(frame_bounds.bottom - 20),
			header_rect.right
		};

		rasterizer_dx9_draw_primitive_quad(&header_rect, header_color);

		draw_string_reset();
		draw_string_set_draw_mode(k_cheevo_title_font, 0, 2, 0, global_real_argb_white, global_real_argb_black, false);

		rectangle2d header_text_bounds = header_rect;
		header_text_bounds.top += k_text_drawing_padding;
		header_text_bounds.bottom += header_line_height + k_text_drawing_padding;

		swprintf_s(string_buffer, L"%s Settings", variant->variant_name);

		rasterizer_draw_unicode_string(&header_text_bounds, string_buffer);

		const uint16 body_column_count = 3;
		const uint16 body_row_count = 2;

		const uint16 body_column_width = (body_rect.right - body_rect.left) / body_column_count;
		const uint16 body_row_height = (body_rect.bottom - body_rect.top) / body_row_count;

		int16 current_cell_index = 0;

		const wchar_t* headers[body_column_count * body_row_count] = {
			L"Match",
			L"Team",
			L"Equipment/Vehicle",
			L"Game Type",
			L"Cartographer",
			L"Players",
		};

		const e_variant_setting_category_type variant_categories[body_column_count * body_row_count]
		{
			(e_variant_setting_category_type)(_variant_setting_category_type_match_ctf + variant->variant_game_engine_index - 1),
			(e_variant_setting_category_type)(_variant_setting_category_type_team_ctf + variant->variant_game_engine_index - 1),
			_variant_setting_category_type_equipment,
			(e_variant_setting_category_type)(_variant_setting_category_type_game_ctf + variant->variant_game_engine_index - 1),
			_variant_setting_category_type_cartographer_settings,
			_variant_setting_category_type_players
		};

		for (uint16 column = 0; column < body_column_count; ++column)
		{
			int16 current_column_left = body_rect.left + (body_column_width * column);
			int16 current_column_right = current_column_left + body_column_width;
			for (uint16 row = 0; row < body_row_count; ++row)
			{
				int16 current_row_top = body_rect.top + (body_row_height * row);
				int16 current_row_bottom = current_row_top + body_row_height;

				rectangle2d cell_rect = 
				{
				current_row_top,
				current_column_left,
				current_row_bottom,
				current_column_right
				};

				rectangle2d cell_header_rect =
				{
					cell_rect.top,
					cell_rect.left,
					(int16)(cell_rect.top + header_line_height),
					cell_rect.right
				};

				rectangle2d cell_body_rect =
				{
					cell_header_rect.bottom,
					cell_rect.left,
					cell_rect.bottom,
					cell_rect.right
				};

				rasterizer_dx9_draw_primitive_quad(&cell_header_rect, header_color);
				rasterizer_dx9_draw_primitive_quad(&cell_body_rect, body_color);

				draw_string_reset();
				draw_string_set_draw_mode(k_cheevo_title_font, 0, 2, 0, global_real_argb_white, global_real_argb_black, false);

				swprintf_s(string_buffer, L"%s", headers[current_cell_index]);
				rasterizer_draw_unicode_string(&cell_header_rect, string_buffer);

				draw_string_reset();
				draw_string_set_draw_mode(k_status_text_font, 0, 0, 0, global_real_argb_white, global_real_argb_black, false);

				cell_body_rect.left += k_text_drawing_padding;

				// grab the category reference if the return is null it is a custom category.
				s_variant_setting_edit_reference* category_reference = multiplayer_variant_settings_interface_get_category_reference(variant_categories[current_cell_index]);

				if (category_reference)
				{
					for (int32 i = 0; i < category_reference->options.count; ++i)
					{
						s_text_value_pair_definition* text_pair = (s_text_value_pair_definition*)tag_get_fast(category_reference->options[i]->index);

						int32 setting_value = multiplayer_variant_settings_interface_get_variant_parameter_value(variant, text_pair->parameter);
						s_text_value_pair_reference_new* setting_label = multiplayer_variant_settings_interface_get_variant_parameter_label(text_pair, setting_value);

						c_maximum_interface_text label_buffer;
						c_maximum_interface_text value_buffer;

						string_list_get_normal_string(text_pair->string_list.index, text_pair->title_text, &label_buffer);

						// format the variant parameter text, if the setting_label is nullptr that means it is a
						// integer parsing parameter just print the int
						if (setting_label)
						{
							string_list_get_normal_string(text_pair->string_list.index, setting_label->label_string, &value_buffer);
							swprintf_s(string_buffer, L"%s: %s", label_buffer.get_string(), value_buffer.get_string());

						}
						else
						{
							swprintf_s(string_buffer, L"%s: %d", label_buffer.get_string(), setting_value);
						}


						rasterizer_draw_unicode_string(&cell_body_rect, string_buffer);

						cell_body_rect.top += body_line_height + k_text_drawing_padding;
					}

					// merge the equipment and vehicle categories so we can keep the 3x2 grid size
					if (variant_categories[current_cell_index] == _variant_setting_category_type_equipment)
					{
						category_reference = multiplayer_variant_settings_interface_get_category_reference(_variant_setting_category_type_vehicles);
						if (category_reference)
						{
							for (int32 i = 0; i < category_reference->options.count; ++i)
							{
								s_text_value_pair_definition* text_pair = (s_text_value_pair_definition*)tag_get_fast(category_reference->options[i]->index);

								int32 setting_value = multiplayer_variant_settings_interface_get_variant_parameter_value(variant, text_pair->parameter);
								s_text_value_pair_reference_new* setting_label = multiplayer_variant_settings_interface_get_variant_parameter_label(text_pair, setting_value);

								c_maximum_interface_text label_buffer;
								c_maximum_interface_text value_buffer;

								string_list_get_normal_string(text_pair->string_list.index, text_pair->title_text, &label_buffer);

								// format the variant parameter text, if the setting_label is nullptr that means it is a
								// integer parsing parameter just print the int
								if (setting_label)
								{
									string_list_get_normal_string(text_pair->string_list.index, setting_label->label_string, &value_buffer);
									swprintf_s(string_buffer, L"%s: %s", label_buffer.get_string(), value_buffer.get_string());

								}
								else
								{
									swprintf_s(string_buffer, L"%s: %d", label_buffer.get_string(), setting_value);
								}

								rasterizer_draw_unicode_string(&cell_body_rect, string_buffer);

								cell_body_rect.top += body_line_height + k_text_drawing_padding;
							}
						}
					}
				}
				else if (variant_categories[current_cell_index] == _variant_setting_category_type_cartographer_settings)
				{
					for (uint32 i = k_variant_setting_parameter_type_base_count + 1; i < k_variant_setting_parameter_type_base_count + 1 + k_variant_setting_parameter_type_cartographer_count; ++i)
					{
						e_variant_setting_parameter_type type = (e_variant_setting_parameter_type)i;

						wchar_t title_buffer[512];
						wchar_t value_buffer[512];

						int32 setting_value = multiplayer_variant_settings_interface_get_variant_parameter_value(variant, type);

						multiplayer_variant_settings_interface_get_custom_variant_parameter_title(nullptr, type, title_buffer);
						multiplayer_variant_settings_interface_get_custom_variant_parameter_label(nullptr, type, setting_value, value_buffer);

						swprintf_s(string_buffer, NUMBEROF(string_buffer), L"%s: %s", title_buffer, value_buffer);

						rasterizer_draw_unicode_string(&cell_body_rect, string_buffer);

						cell_body_rect.top += body_line_height + k_text_drawing_padding;
					}
				}

				current_cell_index++;
			}
		}
	}
}

void render_main_game_time_debug(void)
{
#ifdef MAIN_GAME_TIME_DEBUG
	const s_rasterizer_globals* rasterizer_globals = rasterizer_globals_get();
	const int16 line_height = get_text_size_from_font_cache(k_netdebug_text_font);

	rectangle2d bounds;
	wchar_t main_game_time_debug_text[512];

	swprintf_s(main_game_time_debug_text, ARRAYSIZE(main_game_time_debug_text),
		L"dt default: %.6f dt performance counter: %.6f",
		g_main_game_time_debug.dt_default,
		g_main_game_time_debug.dt_performance_counter
	);

	rasterizer_get_frame_bounds(&bounds);
	bounds.top = (int16)(256 * rasterizer_globals->ui_scale);
	bounds.left = (int16)(64 * rasterizer_globals->ui_scale);
	bounds.bottom = bounds.top + line_height;

	real_argb_color text_color_console = *global_real_argb_white;
	text_color_console.alpha *= (75.f / 100.f);

	draw_string_reset();
	draw_string_set_draw_mode(k_netdebug_text_font, 0, 0, 0, &text_color_console, global_real_argb_black, false);
	draw_string_set_format(0, 0, 0, false);

	rasterizer_draw_unicode_string(&bounds, main_game_time_debug_text);
#endif
}

void render_netdebug_text(void)
{

	if (
#ifndef IMGUI_DISABLE
		ImGuiHandler::g_network_stats_overlay != _network_stats_display_none
#else
		false
#endif
	)
	{
		c_network_session* session = NULL;
		if (network_life_cycle_in_squad_session(&session))
		{
			s_simulation_player_netdebug_data netdebug_data_default{};
			s_simulation_player_netdebug_data* netdebug_data = &netdebug_data_default;
			s_observer_channel* observer_channel = NULL;
			int32 observer_channel_index;

			bool available = session->is_session_class_online()
				&& session->session_mode() == _network_session_mode_in_game;
			if (!available)
				return;

			if (!session->is_host())
			{
				observer_channel_index = session->m_session_peers[session->get_session_host_peer_index()].observer_channel_index;
				if (observer_channel_index != NONE)
				{
					observer_channel = &session->m_network_observer->m_observer_channels[observer_channel_index];

					netdebug_data->client_rtt_msec = (int16)observer_channel->net_rtt;
					netdebug_data->client_packet_rate = (int16)(observer_channel->stream_packet_rate * 10.f);
					netdebug_data->client_throughput = (int16)((observer_channel->throughput_bps * 10.f) / 1024.f);
					netdebug_data->client_packet_loss_percentage = (int16)(observer_channel->packet_loss_statistics.average_values_in_window() * 100.f);

					// NOT UPDATED IN REAL-TIME
					//s_network_session_peer* membership_peer = session->get_peer_membership(session->get_local_peer_index());
					//for (int32 i = 0; i < k_number_of_users; i++)
					//{
					//	if (membership_peer->local_players_indexes[i] != NONE)
					//	{
					//		//netdebug_data = game_engine_get_netdebug_data(membership_peer->local_players_indexes[i]);
					//		break;
					//	}
					//}
				}
			}

			const s_rasterizer_globals* rasterizer_globals = rasterizer_globals_get();
			const int16 line_height = get_text_size_from_font_cache(k_netdebug_text_font);

			rectangle2d bounds;
			wchar_t netdebug_text[512];
			
			rasterizer_get_frame_bounds(&bounds);
			bounds.top = (int16)(8 * rasterizer_globals->ui_scale);
			bounds.left = bounds.right - (int16)(1070 * rasterizer_globals->ui_scale);
			bounds.bottom = bounds.top + line_height;

			real_argb_color text_color_console = *global_real_argb_white;
			text_color_console.alpha *= (65.f / 100.f);

			swprintf_s(netdebug_text, ARRAYSIZE(netdebug_text),
				L"[up^ rtt: %3d msec, pck rate: %.1f, throughput: %.3f bps, loss: %3d %%]",
				netdebug_data->client_rtt_msec,
				(real32)netdebug_data->client_packet_rate / 10.f,
				((real32)netdebug_data->client_throughput / 10.f) * 1024.f,
				netdebug_data->client_packet_loss_percentage
			);

			draw_string_reset();
			draw_string_set_draw_mode(k_netdebug_text_font, 0, 0, 0, &text_color_console, global_real_argb_black, false);

			draw_string_set_format(0, 0, 0, false);
			rasterizer_draw_unicode_string(&bounds, netdebug_text);
		}
	}
}
