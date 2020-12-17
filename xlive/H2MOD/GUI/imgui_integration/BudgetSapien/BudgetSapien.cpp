#include "H2MOD/GUI/imgui_integration/imgui_handler.h"
#include "H2MOD/Tags/TagInterface.h"
#include "H2MOD/GUI/imgui_integration/BudgetSapien/BudgetSapienHelper.h"
#include "H2MOD.h"
#include "imgui_internal.h"

namespace imgui_handler {
	namespace BudgetSapien
	{
		void RenderMainMenu()
		{
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open"))
					{
						//Do something
					}
					if (ImGui::MenuItem("Save"))
					{
						//Do something
					}
					ImGui::EndMenu();
				}

				ImGui::EndMainMenuBar();
			}
		}
		void RenderTreeView()
		{
			
		}
		void Render(bool* p_open)
		{
			RenderMainMenu();
			ImGuiIO& io = ImGui::GetIO();
			RECT rect;
			::GetClientRect(get_HWND(), &rect);
			io.DisplaySize = ImVec2((float)(rect.right - rect.left), (float)(rect.bottom - rect.top));
			ImGuiWindowFlags window_flags = 0;
			window_flags |= ImGuiWindowFlags_NoCollapse;
			window_flags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
			
			//window_flags |= ImGuiWindowFlags_MenuBar;
			//ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_::ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
			//ImGui::PushFont(font2);
			ImGui::SetNextWindowSize(ImVec2(650, 530), ImGuiCond_Appearing);
			ImGui::SetNextWindowSizeConstraints(ImVec2(610, 530), ImVec2(1920, 1080));
			if (h2mod->GetMapType() == MainMenu)
				ImGui::SetNextWindowBgAlpha(1);
			if (ImGui::Begin("Tree View", p_open, window_flags))
			{
				for(auto &it : BudgetSapienHelper::getCurrentTreeNodes())
				{
					if(ImGui::TreeNode(it.Name.c_str()))
					{
						if(it.Children.size())
						{
							ImGui::Indent();
							for(auto &cit : it.Children)
							{
								ImGui::Text(cit.Name.c_str());
								if(ImGui::IsItemHovered()){
									ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
								}
								if (ImGui::IsItemClicked()) {
									BudgetSapienHelper::SelectTag(cit);
								}
								//ImGui::TreePop();
							}
							ImGui::Unindent();
						}
						ImGui::TreePop();
					}
				}
			}
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 8));
			//ImGui::PushFont(font2);
			ImGui::SetNextWindowSize(ImVec2(650, 530), ImGuiCond_Appearing);
			ImGui::SetNextWindowSizeConstraints(ImVec2(610, 530), ImVec2(1920, 1080));
			if (h2mod->GetMapType() == MainMenu)
				ImGui::SetNextWindowBgAlpha(1);
			if(ImGui::Begin("Editor", p_open, window_flags))
			{
				ImGui::Text(BudgetSapienHelper::GetSelectedTag().Name.c_str());
				ImGui::Text(BudgetSapienHelper::GetSelectedTag().Instance.type.as_string().c_str());
				ImGui::Text(BudgetSapienHelper::GetSelectedTag().Instance.datum_index.ToString().c_str());
			}
		}
		void Open()
		{
			BudgetSapienHelper::GenerateTreeNodesTagView();
		}
		void Close()
		{

		}
	}
}
