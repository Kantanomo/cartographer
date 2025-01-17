#include "stdafx.h"

#include "ImGui_CartographerStyle.h"

const static ImVec2 wPadding(20, 10);
const static ImVec2 fPadding(8, 3);
const static ImVec2 iSpacing(6, 6);
const static ImVec2 iiSpacing(6, 6);
const static float indentSpacing = 20.0f;
const static float ScrollBarSize = 16.0f;
const static float GrabMinSize = 13.0f;

const static float WindowBorderSize = 0.0f;
const static float ChildBorderSize = 0.0f;
const static float PopupBorderSize = 0.0f;
const static float FrameBorderSize = 0.0f;
const static float TabBorderSize = 0.0f;

const static float WindowRound = 0.0f;
const static float ChildRounding = 0.0f;
const static float FrameRounding = 0.0f;
const static float PopupRounding = 0.0f;
const static float ScrollbarRounding = 0.0f;
const static float GrabRounding = 0.0f;
const static float TabRounding = 0.0f;

const static ImVec2 WindowTitleAlign(0.0f, 0.50f);
const static ImVec2 ButtonTextAlign(0.50f, 0.50f);
const static ImVec2 SelectabletextAlign(0.0f, 0.50f);

// real_vector4d used instead of ImVec4 so it doesn't create a contructor
const real_vector4d k_imgui_style_colors[ImGuiCol_COUNT]
{
	{0.76f, 0.89f, 1.0f, 1.00f},
	{0.20f, 0.27f, 0.35f, 1.00f},
	{0.04f, 0.09f, 0.17f, 0.81f},
	{0.03f, 0.06f, 0.12f, 0.00f},
	{0.03f, 0.06f, 0.12f, 1.00f},
	{0.22f, 0.56f, 0.87f, 1.00f},
	{0.00f, 0.00f, 0.00f, 0.00f},
	{0.13f, 0.20f, 0.32f, 1.00f},
	{0.14f, 0.20f, 0.28f, 1.00f},
	{0.21f, 0.27f, 0.36f, 1.00f},
	{0.03f, 0.06f, 0.12f, 1.00f},
	{0.09f, 0.31f, 0.53f, 1.00f},
	{0.03f, 0.06f, 0.12f, 0.71f},
	{0.12f, 0.16f, 0.25f, 1.00f},
	{0.03f, 0.06f, 0.12f, 1.00f},
	{0.09f, 0.14f, 0.22f, 1.00f},
	{0.14f, 0.20f, 0.28f, 1.00f},
	{0.21f, 0.27f, 0.36f, 1.00f},
	{0.56f, 0.69f, 0.81f, 1.00f},
	{0.30f, 0.37f, 0.49f, 1.00f},
	{0.22f, 0.56f, 0.87f, 1.00f},
	{0.09f, 0.31f, 0.53f, 1.00f},
	{0.19f, 0.44f, 0.65f, 1.00f},
	{0.18f, 0.45f, 0.72f, 1.00f},
	{0.21f, 0.27f, 0.36f, 0.27f},
	{0.14f, 0.20f, 0.28f, 1.00f},
	{0.21f, 0.27f, 0.36f, 1.00f},
	{0.11f, 0.33f, 0.55f, 1.00f},
	{0.18f, 0.45f, 0.72f, 1.00f},
	{0.24f, 0.60f, 0.95f, 1.00f},
	{0.09f, 0.14f, 0.22f, 1.00f},
	{0.14f, 0.20f, 0.28f, 1.00f},
	{0.21f, 0.27f, 0.36f, 1.00f},
	{0.09f, 0.31f, 0.53f, 0.00f},
	{0.09f, 0.14f, 0.22f, 1.00f},
	{0.11f, 0.33f, 0.55f, 1.00f},
	{0.03f, 0.06f, 0.12f, 1.00f},
	{0.09f, 0.14f, 0.22f, 1.00f},
	{0.56f, 0.69f, 0.81f, 1.00f},
	{1.00f, 1.00f, 1.00f, 1.00f},
	{0.56f, 0.69f, 0.81f, 1.00f},
	{1.00f, 1.00f, 1.00f, 1.00f},
	{0.19f, 0.19f, 0.20f, 1.00f},
	{0.31f, 0.31f, 0.35f, 1.00f},    // Prefer using Alpha=1.0 here
	{0.23f, 0.23f, 0.25f, 1.00f},     // Prefer using Alpha=1.0 here
	{0.00f, 0.00f, 0.00f, 0.00f},
	{1.00f, 1.00f, 1.00f, 0.06f},
	{0.09f, 0.31f, 0.53f, 1.00f},
	{0.56f, 0.69f, 0.81f, 1.00f},
	{0.24f, 0.60f, 0.95f, 1.00f},
	{1.00f, 1.00f, 1.00f, 0.70f},
	{0.80f, 0.80f, 0.80f, 0.20f},
	{0.80f, 0.80f, 0.80f, 0.35f},
};

void imgui::Cartographer::StyleColorsMaik(ImGuiStyle* dst)
{
	ImGuiStyle* style = dst != nullptr ? dst : &ImGui::GetStyle();
	csmemcpy(style->Colors, k_imgui_style_colors, sizeof(style->Colors));
	return;
}

void imgui::Cartographer::StyleSetAsDefault(ImGuiStyle* out)
{
	// initialize default window style for each window
	auto& style = out != nullptr ? *out : ImGui::GetStyle();

	style.WindowPadding = wPadding;
	style.FramePadding = fPadding;
	style.ItemSpacing = iSpacing;
	style.ItemInnerSpacing = iiSpacing;
	style.IndentSpacing = indentSpacing;

	style.ScrollbarSize = ScrollBarSize;
	style.GrabMinSize = GrabMinSize;

	style.WindowBorderSize = WindowBorderSize;
	style.ChildBorderSize = ChildBorderSize;
	style.PopupBorderSize = PopupBorderSize;
	style.FrameBorderSize = FrameBorderSize;

	style.WindowRounding = WindowRound;
	style.ChildRounding = ChildRounding;
	style.PopupRounding = PopupRounding;
	style.ScrollbarRounding = ScrollbarRounding;
	style.GrabRounding = GrabRounding;
	style.TabRounding = TabRounding;

	style.WindowTitleAlign = WindowTitleAlign;
	style.ButtonTextAlign = ButtonTextAlign;
	style.SelectableTextAlign = SelectabletextAlign;

	StyleColorsMaik(&style);
}

void imgui::Cartographer::PushStyle()
{
	// initialize default window style for each window
	auto& ImDefaultStyle = ImGui::GetStyle();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, wPadding);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, fPadding);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, iSpacing);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, iiSpacing);
	ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, indentSpacing);

	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, ScrollBarSize);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, GrabMinSize);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, WindowBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, ChildBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, PopupBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, FrameBorderSize);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, WindowRound);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ChildRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, PopupRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, ScrollbarRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, GrabRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, TabRounding);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, WindowTitleAlign);
	ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ButtonTextAlign);
	ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, SelectabletextAlign);
}

void imgui::Cartographer::PopStyle()
{
	ImGui::PopStyleVar(20);
}
