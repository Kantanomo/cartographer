#include "stdafx.h"
#include "imgui_handler.h"


#include "game/player_constants.h"
#include "input/input_abstraction.h"
#include "input/input_windows.h"

#include "H2MOD/Modules/Shell/Config.h"
#include "H2MOD/Modules/Shell/Startup/Startup.h"

#include "imgui.h"

const char* k_motd_url = "http://www.halo2pc.com/motd.png";
const wchar_t* k_motd_filename = L"motd.png";

extern int notify_xlive_ui;

namespace ImGuiHandler
{
	namespace ImMOTD {

		namespace
		{
			bool load_complete = false;
			bool download_success = false;
			bool download_complete = false;
			std::atomic<bool> motd_texture_load_in_progress = false;
			int X;
			int Y;
		}
		bool DownloadMOTD(const wchar_t* file_path, e_imgui_aspect_ratio ratio)
		{
			FILE* fp = NULL;
			CURL* curl;
			CURLcode res;

			bool success = false;

			curl = curl_interface_init_no_verify();
			if (curl)
			{
				fp = _wfopen(file_path, L"wb");
				curl_easy_setopt(curl, CURLOPT_URL, k_motd_url);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
				curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
				curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
				res = curl_easy_perform(curl);
				if (res == CURLcode::CURLE_OK)
				{
					fseek(fp, 0, SEEK_END);
					int size = ftell(fp);
					if (size > 10252) 
					{
						success = true;
					}
					fclose(fp);
				}
				else
				{
					fclose(fp);
					// _wremove(file_path.c_str());
				}
				curl_easy_cleanup(curl);
			}

			return success;
		}
		bool LoadMOTD(const wchar_t* file_path, e_imgui_aspect_ratio ratio)
		{
			if (std::filesystem::exists(file_path))
			{
				return ImGuiHandler::LoadTextureFromFile(file_path, patch_notes, &X, &Y);
			}
			return false;
		}
		void DownloadAndLoadMOTD()
		{
			motd_texture_load_in_progress = true;

			c_static_wchar_string<MAX_PATH> motd_path_wide;
			const wchar_t* motd_initial_path = H2Portable ? L".\\" : g_h2_appdata_local_path;	// Use process path if portable, Appdata otherwise
			motd_path_wide.set(motd_initial_path);
			motd_path_wide.append(k_motd_filename);

			const ImVec2 size_imgui = ImGui::GetMainViewport()->WorkSize;
			const real_point2d size = { size_imgui.x, size_imgui.y };

			const e_imgui_aspect_ratio aspect_ratio = GetAspectRatio(&size);

			if (!download_complete)
			{
				download_success = DownloadMOTD(motd_path_wide.get_string(), aspect_ratio);
				download_complete = true;
			}
			load_complete = LoadMOTD(motd_path_wide.get_string(), aspect_ratio);
			motd_texture_load_in_progress = false;
		}

		void Render(bool* p_open)
		{
			if (!download_complete
				|| motd_texture_load_in_progress)
				return;

			// if MOTD cannot be loaded, just close the menu
			if (!load_complete)
			{
				ImGuiHandler::ToggleWindow(k_motd_window_name);
				return;
			}

			if (ImGuiHandler::GetTexture(patch_notes) == nullptr)
			{
				load_complete = false;
				std::thread(DownloadAndLoadMOTD).detach();
				return;
			}

			//ImVec2 Resolution(
			//	ImGui::GetIO().DisplaySize.x,
			//	ImGui::GetIO().DisplaySize.y
			//);
			//ImVec2 Dimensions;
			//switch(ratio)
			//{
			//	case four_three: 
			//		Dimensions = ImVec2(
			//			1184, 745
			//		);
			//	break;
			//	default:
			//	case sixten_nine:
			//		Dimensions = ImVec2(
			//			1786, 745
			//		);
			//	break;
			//
			//}

			//float MinScale = 1.5f;

			//ImVec2 Scale(
			//	fmaxf(MinScale, (Dimensions.x / Resolution.x)),
			//	fmaxf(MinScale, (Dimensions.y / Resolution.y))
			//);
			///*Scale.x = fmaxf(MinScale, Scale.x);
			//Scale.y = fmaxf(MinScale, Scale.y);*/
			//float scaledx = Dimensions.x / Scale.x;
			//float scaledy = Dimensions.y / Scale.y;

			//ImVec2 TopLeft(
			//	(ImGui::GetIO().DisplaySize.x / 2) - (scaledx / 2),
			//	(ImGui::GetIO().DisplaySize.y / 2) - (scaledy / 2)
			//);
			//ImVec2 BottomRight(
			//	(ImGui::GetIO().DisplaySize.x / 2) + (scaledx / 2),
			//	(ImGui::GetIO().DisplaySize.y / 2) + (scaledy / 2)
			//);

			ImGuiWindowFlags window_flags = 0
				| ImGuiWindowFlags_NoDecoration
				| ImGuiWindowFlags_NoSavedSettings
				| ImGuiWindowFlags_NoBackground
				;

			ImGuiIO& io = ImGui::GetIO();
			const ImGuiViewport* viewport = ImGui::GetMainViewport();

			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			if (ImGui::Begin("##motd", NULL, window_flags))
			{
				ImTextureID texId = (ImTextureID)ImGuiHandler::GetTexture(patch_notes);
				ImGui::Image(texId, ImGui::GetIO().DisplaySize);

				for (uint16 gamepad_index = 0; gamepad_index < k_number_of_users; gamepad_index++)
				{
					if (input_abstraction_globals->input_has_gamepad[gamepad_index] &&
						input_get_gamepad_state(gamepad_index)->button_frames_down[_xinput_gamepad_a])
					{
						ImGuiHandler::ToggleWindow(k_motd_window_name);
					}
				}

				if (ImGui::IsItemClicked())
				{
					ImGuiHandler::ToggleWindow(k_motd_window_name);
				}
				else
				{
					for (int32 i = ImGuiKey_NamedKey_BEGIN; i < ImGuiKey_NamedKey_END; ++i)
					{
						if (ImGui::IsKeyPressed((ImGuiKey)i))
						{
							ImGuiHandler::ToggleWindow(k_motd_window_name);
							break;
						}
					}
				}
			}
			ImGui::PopStyleVar(2);
			ImGui::End();
		}
		void Open()
		{
			if (!download_complete)
				std::thread(DownloadAndLoadMOTD).detach();
		}
		void Close()
		{
			notify_xlive_ui = 0;
			XUserSignInSetStatusChanged(0);
		}
	}
}
