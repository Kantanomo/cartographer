#include "stdafx.h"

#include "imgui.h"
#include "imgui_handler.h"

#include "Blam/Engine/game/game.h"
#include "H2MOD/Modules/Input/PlayerControl.h"
#include "Util/Hooks/Hook.h"

namespace ImGuiHandler
{
	namespace ImMessageBox
	{
		std::string windowName = "messagebox";

		namespace
		{
			std::string message;
			float extra_height = 0;
			char window_title[32] = "Message";
			char default_label[32] = "Ok";
			char secondary_label[32] = "";
			char tertiary_label[32] = "";
			std::function<void()> default_callback = nullptr;
			std::function<void()> secondary_callback = nullptr;
			std::function<void()> tertiary_callback = nullptr;
			bool default_close = true;
			bool secondary_close = true;
			bool tertiary_close = true;
		}
		void Render(bool* p_open)
		{
			bool open = *p_open;
			ImGuiIO& io = ImGui::GetIO();
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_NoResize;
			//window_flags |= ImGuiWindowFlags_MenuBar;
			ImGui::SetNextWindowPos(ImVec2(viewport->WorkSize.x * 0.5f, viewport->WorkSize.y * 0.5f), ImGuiCond_::ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
			//ImGui::PushFont(font2);
			ImGui::SetNextWindowSize(ImVec2(650, (250 + extra_height)), ImGuiCond_Appearing);
			ImGui::SetNextWindowSizeConstraints(ImVec2(610, (250 + extra_height)), ImVec2(1920, 1080));
			if (game_is_ui_shell())
				ImGui::SetNextWindowBgAlpha(1);
			if (ImGui::Begin(window_title, NULL, window_flags))
			{
				ImGui::TextWrapped(message.c_str());
				ImVec2 textSize = ImGui::CalcTextSize(message.c_str());
				float lineHeight = ImGui::GetTextLineHeightWithSpacing(); // Get line height with spacing
				int numLines = static_cast<int>(textSize.y / lineHeight); // Calculate number of wrapped lines
				float extra_height_ = numLines* lineHeight;
				extra_height = extra_height_;
				
				char label_buffer[36];
				if (secondary_label[0] == '\0')
				{
					ImGui::SetCursorPosY(190 + extra_height);
					sprintf(label_buffer, "%s##%s", default_label, "D1");
					if (ImGui::Button(label_buffer, ImVec2(610, 50)))
					{
						if (default_callback != nullptr)
							default_callback();
						if(default_close)
							ImGuiHandler::ToggleWindow(ImGuiHandler::ImMessageBox::windowName);
					}
				}
				else if (tertiary_label[0] == '\0')
				{
					ImGui::SetCursorPosY(190 + extra_height);
					sprintf(label_buffer, "%s##%s", default_label, "D1");
					if (ImGui::Button(label_buffer, ImVec2(300, 50)))
					{
						if (default_callback != nullptr)
							default_callback();
						if(default_close)
							ImGuiHandler::ToggleWindow(ImGuiHandler::ImMessageBox::windowName);
					}
					ImGui::SetCursorPosY(190 + extra_height);
					ImGui::SetCursorPosX(335);
					sprintf(label_buffer, "%s##%s", secondary_label, "D2");
					if (ImGui::Button(label_buffer, ImVec2(300, 50)))
					{
						if (secondary_callback != nullptr)
							secondary_callback();
						if(secondary_close)
							ImGuiHandler::ToggleWindow(ImGuiHandler::ImMessageBox::windowName);
					}
				}
				else
				{
					ImGui::SetCursorPosY(190 + extra_height);
					sprintf(label_buffer, "%s##%s", default_label, "D1");
					if (ImGui::Button(label_buffer, ImVec2(195, 50)))
					{
						if (default_callback != nullptr)
							default_callback();
						if(default_close)
							ImGuiHandler::ToggleWindow(ImGuiHandler::ImMessageBox::windowName);
					}
					ImGui::SetCursorPosY(190 + extra_height);
					ImGui::SetCursorPosX(228.5);
					sprintf(label_buffer, "%s##%s", secondary_label, "D2");
					if (ImGui::Button(label_buffer, ImVec2(195, 50)))
					{
						if (secondary_callback != nullptr)
							secondary_callback();
						if(secondary_close)
							ImGuiHandler::ToggleWindow(ImGuiHandler::ImMessageBox::windowName);
					}
					ImGui::SetCursorPosY(190 + extra_height);
					ImGui::SetCursorPosX(435);
					sprintf(label_buffer, "%s##%s", tertiary_label, "D3");
					if (ImGui::Button(label_buffer, ImVec2(195, 50)))
					{
						if (tertiary_callback != nullptr)
							tertiary_callback();
						if(tertiary_close)
							ImGuiHandler::ToggleWindow(ImGuiHandler::ImMessageBox::windowName);
					}
				}
			}
			// Pop style var
			ImGui::PopStyleVar();
			ImGui::End();
			
		}
		void SetMessage(std::string Message)
		{
			message = Message;
		}
		void SetTitle(const char* title)
		{
			size_t len = strlen(title);
			if (len > 31)
				len = 31;
			memcpy(window_title, title, len);
			window_title[len] = '\0';
		}
		void SetDefaultOption(const char* label, bool close_window, const std::function<void()>& callback_function)
		{
			size_t len = strlen(label);
			if (len > 31)
				len = 31;
			memcpy(default_label, label, len);
			default_label[len] = '\0';
			default_close = close_window;
			default_callback = callback_function;
		}
		void SetSecondaryOption(const char* label, bool close_window, const std::function<void()>& callback_function)
		{
			size_t len = strlen(label);
			if (len > 31)
				len = 31;
			memcpy(secondary_label, label, len);
			secondary_label[len] = '\0';
			secondary_close = close_window;
			secondary_callback = callback_function;
		}
		void SetTertiaryOption(const char* label, bool close_window, const std::function<void()>& callback_function)
		{
			size_t len = strlen(label);
			if (len > 31)
				len = 31;
			memcpy(tertiary_label, label, len);
			tertiary_label[len] = '\0';
			tertiary_close = close_window;
			tertiary_callback = callback_function;
		}
		void Open()
		{
			
		}
		void Close()
		{
			extra_height = 0;
			SetDefaultOption("Ok", nullptr);
			SetSecondaryOption("", nullptr);
			SetTertiaryOption("", nullptr);
			SetTitle("Message");
		}
	}
}