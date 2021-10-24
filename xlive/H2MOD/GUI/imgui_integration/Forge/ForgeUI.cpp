#include "ForgeUI.h"

#include "H2MOD.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "H2MOD/GUI/imgui_integration/imgui_handler.h"
#include "H2MOD/Modules/Input/PlayerControl.h"
#include "H2MOD/Modules/Utils/Utils.h"
#include "H2MOD/Tags/TagInterface.h"
#include "Util/Hooks/Hook.h"

namespace forge_ui
{
	namespace
	{
		datum selectedObject;

		void updateSelectedObject(datum index)
		{
			if (selectedObject == index)
				return;
			selectedObject = index;
		}

		bool isObjectSelected(datum index)
		{
			return (selectedObject == index);
		}

		s_object_data_definition* getSelectedObject()
		{
			auto it = new s_data_iterator<s_object_header>(get_objects_header());
			while (it->get_next_datum()) {
				auto i = it->get_current_datum();
				datum i_datum = i->datum_salt << 16;
				i_datum += *(short*)&i->flags;
				if (i_datum == selectedObject)
					return reinterpret_cast<s_object_data_definition*>(it->get_current_datum()->object);
			}
			return nullptr;
		}
		float WidthPercentage(float percent)
		{
			auto Width = ImGui::GetWindowContentRegionWidth();
			if (ImGui::GetColumnsCount() > 1)
				Width = ImGui::GetColumnWidth();

			return Width * (percent / 100.0f);
		}
		void render_real_vector_3d(real_point3d* vector, char* title)
		{
			ImGui::BeginGroupPanel(title, ImVec2(350, ImGui::GetContentRegionAvail().y));

			ImGui::PushItemWidth(WidthPercentage(75));
			ImGui::SliderFloat("Position X", &vector->x, -500, 500, "");
			ImGui::PushItemWidth(WidthPercentage(13));
			ImGui::InputFloat("##PX", &vector->x, 0, 3);

			ImGui::PushItemWidth(WidthPercentage(75));
			ImGui::SliderFloat("Position Y", &vector->y, -500, 500, "");
			ImGui::PushItemWidth(WidthPercentage(13));
			ImGui::InputFloat("##PY", &vector->y, 0, 3);

			ImGui::PushItemWidth(WidthPercentage(75));
			ImGui::SliderFloat("Position Z", &vector->z, -500, 500, "");
			ImGui::PushItemWidth(WidthPercentage(13));
			ImGui::InputFloat("##PZ", &vector->z, 0, 3);
			real_vector3d u = getSelectedObject()->up;
			real_vector3d f = getSelectedObject()->orientation;
			real_vector3d p = getSelectedObject()->position;
			
			//object_set_position(selectedObject, 0, &p, 0, 0);

			ImGui::EndGroupPanel();
		}
	}
	void render_objects()
	{
		auto it = new s_data_iterator<s_object_header>(get_objects_header());
		while (it->get_next_datum())
		{
			auto i = it->get_current_datum();
			//if (!(i->type & FLAG(e_object_type::biped)))
			//{
				datum i_datum = i->datum_salt << 16;
				i_datum += *(short*)&i->flags;
				auto object = reinterpret_cast<s_object_data_definition*>(i->object);
				auto t_name = tags::get_tag_name(object->tag_definition_index);
				auto t_instance = tags::datum_to_instance(object->tag_definition_index);
				auto tnode = std::string(t_name.substr(t_name.find_last_of("/\\") + 1));
				//tnode += " - " + IntToString<datum>(t_instance->datum_index, std::hex);
				tnode += " - " + t_instance->type.as_string();
				//tnode += "##" + std::to_string(i->datum_salt);

				if (!isObjectSelected(i_datum))
					ImGui::Text(tnode.c_str());
				else
					ImGui::TextColored(ImVec4(1, 1, 1, 1), tnode.c_str());

				if(ImGui::IsItemClicked())
				{
					selectedObject = i_datum;
					LOG_INFO_GAME("[{}] Selected Item {:x} {}", __FUNCSIG__, selectedObject, t_name);
				}
			//}
		}

	}

	 __declspec(noinline) void render_properties()
	{
		auto object = getSelectedObject();
		if(object != nullptr)
		{
			render_real_vector_3d(&object->position, "Position");
		}
	}

	void render_main(bool* p_open)
	{
		ImGuiIO& io = ImGui::GetIO();
		RECT rect;
		::GetClientRect(imgui_handler::get_HWND(), &rect);
		io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
		ImGuiWindowFlags window_flags = 0;
		window_flags |= ImGuiWindowFlags_NoCollapse;
		window_flags |= ImGuiWindowFlags_NoResize;
		window_flags |= ImGuiWindowFlags_MenuBar;
		window_flags |= ImGuiWindowFlags_NoScrollbar;

		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_::ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
		ImGui::SetNextWindowSize(ImVec2(800, 650), ImGuiCond_Appearing);
		ImGui::SetNextWindowSizeConstraints(ImVec2(610, 650), ImVec2(1920, 1080));

		/*if(h2mod->GetEngineType() != Multiplayer)
		{
			p_open = false;
			return;
		}*/
		if(ImGui::Begin("ForgeWindow", p_open, window_flags))
		{
			auto contentHeight = ImGui::GetContentRegionAvail().y;//ImGui::GetWindowHeight() - ImGui::GetCurrentContext()->CurrentWindow->MenuBarHeight() - ImGui::GetStyle().FramePadding.y;
			ImGui::BeginChild("ObjectsList", ImVec2(350, contentHeight));
			render_objects();
			ImGui::EndChild();

			ImGui::SameLine(400);

			ImGui::BeginChild("Properties");
			render_properties();
			ImGui::EndChild();
		}
	}
	void open()
	{
		WriteValue<byte>(Memory::GetAddress(0x9712cC), 1);
		imgui_handler::ImGuiToggleInput(true);
		PlayerControl::DisableLocalCamera(true);
	}
	void close()
	{
		WriteValue<byte>(Memory::GetAddress(0x9712cC), 0);
		imgui_handler::ImGuiToggleInput(false);
		PlayerControl::DisableLocalCamera(false);
	}
}
