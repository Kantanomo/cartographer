#include "stdafx.h"

#include "WeaponOffsetsStringTable.h"

#include "cache/cache_files.h"

#include "game/game.h"
#include "H2MOD/GUI/ImGui_Integration/ImGui_Handler.h"
#include "H2MOD/Modules/WeaponOffsets/WeaponOffsetConfig.h"

#include "imgui.h"

/* globals */

c_static_string<256> g_weapon_offsets_temp_string;

/* prototypes */

const char* weapon_offsets_get_string(e_weapon_offsets_string string, const char* id = NULL);

namespace ImGuiHandler {
	namespace WeaponOffsets {
		s_weapon_custom_offset g_weap_offset_data[16]
		{
			{{{0,0,0}}, NULL}
		};

		void ApplyOffset(size_t weapon)
		{
			if (g_weap_offset_data[weapon].tag != nullptr)
			{
				g_weap_offset_data[weapon].tag->weapon.first_person_weapon_offset = g_weap_offset_data[weapon].modified_offset;
			}
		}
		namespace
		{
			void OffsetMenu(e_weapon_offset_weapon weapon, const char* slider, e_weapon_offsets_string text, float& offset, float default_value)
			{
				ImVec2 item_size = ImGui::GetItemRectSize();
				ImVec2 b2_size = ImVec2(WidthPercentage(20), item_size.y);

				std::string sliderId(slider); sliderId += "slider";
				std::string buttonId(slider); buttonId += "button";

				ImGui::Text(weapon_offsets_get_string(text));
				ImGui::PushItemWidth(WidthPercentage(60));
				ImGui::SliderFloat(slider, &offset, -0.15f, 0.15f, ""); ImGui::SameLine();
				if (ImGui::IsItemEdited() && g_weap_offset_data[weapon].tag != nullptr) { ApplyOffset(weapon); }

				ImGui::PushItemWidth(WidthPercentage(20));
				ImGui::InputFloat(sliderId.c_str(), &offset, -0.15f, 0.15f, "%.3f"); ImGui::SameLine();
				if (ImGui::IsItemEdited() && g_weap_offset_data[weapon].tag != nullptr) { ApplyOffset(weapon); }

				ImGui::PushItemWidth(WidthPercentage(20));
				if (ImGui::Button(weapon_offsets_get_string(_weapon_offsets_string_reset, buttonId.c_str()), b2_size))
				{
					offset = default_value;
					if (g_weap_offset_data[weapon].tag != nullptr) { ApplyOffset(weapon); }
				}
				ImGui::PopItemWidth();
			}
			void OffsetSettings()
			{
				static int selectedOption = _weapon_offset_weapon_battle_rifle;

				// Populate weapon string list
				const char* weapons[k_weapon_offsets_weapon_title_count];
				uint32 i = 0;
				for (uint8 string = k_weapon_offsets_weapon_title_first; string <= k_weapon_offsets_weapon_title_last; string++)
				{
					weapons[i] = weapon_offsets_get_string((e_weapon_offsets_string)string);
					++i;
				}

				// Setup combo box menus for each weapon
				ImGui::Combo(weapon_offsets_get_string(_weapon_offsets_string_combo_title), &selectedOption, weapons, k_weapon_offset_weapon_count);
				OffsetMenu((e_weapon_offset_weapon)selectedOption, "##OffsetX", _weapon_offsets_string_weapon_offset_x,
					g_weap_offset_data[selectedOption].modified_offset.x, k_weapon_custom_offset_constant_data[selectedOption].default_offset.x);
				OffsetMenu((e_weapon_offset_weapon)selectedOption, "##OffsetY", _weapon_offsets_string_weapon_offset_y,
					g_weap_offset_data[selectedOption].modified_offset.y, k_weapon_custom_offset_constant_data[selectedOption].default_offset.y);
				OffsetMenu((e_weapon_offset_weapon)selectedOption, "##OffsetZ", _weapon_offsets_string_weapon_offset_z,
					g_weap_offset_data[selectedOption].modified_offset.z, k_weapon_custom_offset_constant_data[selectedOption].default_offset.z);
			}
		}

		void Render(bool* p_open)
		{
			bool open = *p_open;
			
			ImGuiIO& io = ImGui::GetIO();
			ImGuiWindowFlags window_flags = 0
				| ImGuiWindowFlags_NoCollapse
				| ImGuiWindowFlags_AlwaysVerticalScrollbar
				;
			ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_::ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
			ImGui::SetNextWindowSize(ImVec2(450, 320), ImGuiCond_Appearing);
			ImGui::SetNextWindowSizeConstraints(ImVec2(410, 320), ImVec2(1920, 1080));
			if (game_is_ui_shell())
				ImGui::SetNextWindowBgAlpha(1);
			if (ImGui::Begin(weapon_offsets_get_string(_weapon_offsets_string_title), &open, window_flags))
			{
				OffsetSettings();
			}

			ImGui::PopStyleVar();
			ImGui::End();
			if (!open)
			{
				ImGuiHandler::ToggleWindow(k_weapon_offsets_window_name);
			}
		}
		void Open()
		{
			ReadWeaponOffsetConfig(g_weap_offset_data, ARRAYSIZE(g_weap_offset_data));
		}
		void Close()
		{
			SaveWeaponOffsetConfig(g_weap_offset_data, ARRAYSIZE(g_weap_offset_data), false);
		}

		void MapLoad()
		{
			for (size_t i = 0; i < ARRAYSIZE(k_weapon_custom_offset_constant_data); i++)
			{
				datum weap_datum = tag_loaded(_tag_group_weapon, k_weapon_custom_offset_constant_data[i].weapon_path);
				if (weap_datum != NONE)
				{
					g_weap_offset_data[i].tag = (weapon_definition*)tag_get_fast(weap_datum);
					ApplyOffset(i);
				}
			}
		}
		void Initialize()
		{
			WriteDefaultFile(g_weap_offset_data, ARRAYSIZE(g_weap_offset_data));
			ReadWeaponOffsetConfig(g_weap_offset_data, ARRAYSIZE(g_weap_offset_data));
		}
	}
}

/* private code */

const char* weapon_offsets_get_string(e_weapon_offsets_string string, const char* id)
{
	const e_language language = get_current_language();
	const char* result = k_weapon_offsets_string_table[language][string];

	if (id != NULL)
	{
		g_weapon_offsets_temp_string.set(result);
		g_weapon_offsets_temp_string.append("##");
		g_weapon_offsets_temp_string.append(id);
		result = g_weapon_offsets_temp_string.get_string();
	}

	return result;
}
