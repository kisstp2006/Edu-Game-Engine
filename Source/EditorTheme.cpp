#include "EditorTheme.h"

#include <imgui.h>

namespace EGE::EditorTheme
{
	namespace
	{
		void ApplyLayout(ImGuiStyle& style, bool compact)
		{
			style.WindowPadding = compact
				? ImVec2(6.0f, 6.0f)
				: ImVec2(10.0f, 10.0f);
			style.FramePadding = compact
				? ImVec2(6.0f, 3.0f)
				: ImVec2(8.0f, 5.0f);
			style.ItemSpacing = compact
				? ImVec2(6.0f, 4.0f)
				: ImVec2(8.0f, 6.0f);
			style.ItemInnerSpacing = compact
				? ImVec2(4.0f, 3.0f)
				: ImVec2(6.0f, 4.0f);
			style.IndentSpacing = compact ? 18.0f : 21.0f;
			style.ScrollbarSize = compact ? 12.0f : 14.0f;
			style.GrabMinSize = compact ? 9.0f : 11.0f;

			style.WindowRounding = 5.0f;
			style.ChildRounding = 4.0f;
			style.PopupRounding = 5.0f;
			style.FrameRounding = 4.0f;
			style.ScrollbarRounding = 8.0f;
			style.GrabRounding = 3.0f;
			style.TabRounding = 4.0f;

			style.WindowBorderSize = 1.0f;
			style.ChildBorderSize = 1.0f;
			style.PopupBorderSize = 1.0f;
			style.FrameBorderSize = 0.0f;
			style.TabBorderSize = 0.0f;
		}

		void ApplyMidnightPalette(ImGuiStyle& style)
		{
			ImVec4* colors = style.Colors;

			colors[ImGuiCol_Text] =
				ImVec4(0.89f, 0.92f, 0.96f, 1.00f);
			colors[ImGuiCol_TextDisabled] =
				ImVec4(0.43f, 0.48f, 0.57f, 1.00f);
			colors[ImGuiCol_WindowBg] =
				ImVec4(0.055f, 0.065f, 0.085f, 1.00f);
			colors[ImGuiCol_ChildBg] =
				ImVec4(0.065f, 0.075f, 0.098f, 1.00f);
			colors[ImGuiCol_PopupBg] =
				ImVec4(0.075f, 0.087f, 0.115f, 0.98f);
			colors[ImGuiCol_Border] =
				ImVec4(0.18f, 0.21f, 0.28f, 0.80f);
			colors[ImGuiCol_BorderShadow] =
				ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

			colors[ImGuiCol_FrameBg] =
				ImVec4(0.105f, 0.125f, 0.165f, 1.00f);
			colors[ImGuiCol_FrameBgHovered] =
				ImVec4(0.15f, 0.19f, 0.25f, 1.00f);
			colors[ImGuiCol_FrameBgActive] =
				ImVec4(0.18f, 0.23f, 0.31f, 1.00f);

			colors[ImGuiCol_TitleBg] =
				ImVec4(0.043f, 0.052f, 0.070f, 1.00f);
			colors[ImGuiCol_TitleBgActive] =
				ImVec4(0.075f, 0.093f, 0.130f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed] =
				ImVec4(0.043f, 0.052f, 0.070f, 0.85f);
			colors[ImGuiCol_MenuBarBg] =
				ImVec4(0.070f, 0.082f, 0.105f, 1.00f);

			colors[ImGuiCol_ScrollbarBg] =
				ImVec4(0.045f, 0.053f, 0.070f, 0.80f);
			colors[ImGuiCol_ScrollbarGrab] =
				ImVec4(0.20f, 0.24f, 0.31f, 1.00f);
			colors[ImGuiCol_ScrollbarGrabHovered] =
				ImVec4(0.27f, 0.32f, 0.41f, 1.00f);
			colors[ImGuiCol_ScrollbarGrabActive] =
				ImVec4(0.34f, 0.40f, 0.51f, 1.00f);

			const ImVec4 accent(0.22f, 0.58f, 0.95f, 1.00f);
			const ImVec4 accentHovered(0.30f, 0.68f, 1.00f, 1.00f);
			const ImVec4 accentActive(0.16f, 0.48f, 0.88f, 1.00f);

			colors[ImGuiCol_CheckMark] = accentHovered;
			colors[ImGuiCol_SliderGrab] = accent;
			colors[ImGuiCol_SliderGrabActive] = accentHovered;
			colors[ImGuiCol_Button] =
				ImVec4(0.13f, 0.20f, 0.30f, 1.00f);
			colors[ImGuiCol_ButtonHovered] =
				ImVec4(0.18f, 0.36f, 0.56f, 1.00f);
			colors[ImGuiCol_ButtonActive] = accentActive;
			colors[ImGuiCol_Header] =
				ImVec4(0.12f, 0.22f, 0.34f, 0.85f);
			colors[ImGuiCol_HeaderHovered] =
				ImVec4(0.16f, 0.36f, 0.56f, 0.90f);
			colors[ImGuiCol_HeaderActive] =
				ImVec4(0.18f, 0.43f, 0.70f, 1.00f);

			colors[ImGuiCol_Separator] =
				ImVec4(0.17f, 0.20f, 0.27f, 1.00f);
			colors[ImGuiCol_SeparatorHovered] = accent;
			colors[ImGuiCol_SeparatorActive] = accentHovered;
			colors[ImGuiCol_ResizeGrip] =
				ImVec4(0.22f, 0.58f, 0.95f, 0.18f);
			colors[ImGuiCol_ResizeGripHovered] =
				ImVec4(0.22f, 0.58f, 0.95f, 0.55f);
			colors[ImGuiCol_ResizeGripActive] = accentHovered;

			colors[ImGuiCol_Tab] =
				ImVec4(0.075f, 0.095f, 0.135f, 1.00f);
			colors[ImGuiCol_TabHovered] =
				ImVec4(0.18f, 0.38f, 0.60f, 1.00f);
			colors[ImGuiCol_TabActive] =
				ImVec4(0.12f, 0.27f, 0.44f, 1.00f);
			colors[ImGuiCol_TabUnfocused] =
				ImVec4(0.055f, 0.068f, 0.093f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive] =
				ImVec4(0.09f, 0.18f, 0.29f, 1.00f);

			colors[ImGuiCol_DockingPreview] =
				ImVec4(0.22f, 0.58f, 0.95f, 0.55f);
			colors[ImGuiCol_DockingEmptyBg] =
				ImVec4(0.040f, 0.048f, 0.064f, 1.00f);
			colors[ImGuiCol_PlotLines] =
				ImVec4(0.56f, 0.67f, 0.83f, 1.00f);
			colors[ImGuiCol_PlotLinesHovered] =
				ImVec4(1.00f, 0.68f, 0.25f, 1.00f);
			colors[ImGuiCol_PlotHistogram] =
				ImVec4(0.25f, 0.78f, 0.67f, 1.00f);
			colors[ImGuiCol_PlotHistogramHovered] =
				ImVec4(0.38f, 0.90f, 0.76f, 1.00f);
			colors[ImGuiCol_TextSelectedBg] =
				ImVec4(0.22f, 0.58f, 0.95f, 0.35f);
			colors[ImGuiCol_DragDropTarget] =
				ImVec4(0.98f, 0.72f, 0.24f, 0.95f);
			colors[ImGuiCol_NavHighlight] = accentHovered;
			colors[ImGuiCol_NavWindowingHighlight] =
				ImVec4(0.89f, 0.92f, 0.96f, 0.70f);
			colors[ImGuiCol_NavWindowingDimBg] =
				ImVec4(0.02f, 0.03f, 0.05f, 0.70f);
			colors[ImGuiCol_ModalWindowDimBg] =
				ImVec4(0.02f, 0.03f, 0.05f, 0.78f);
		}
	}

	void Apply(std::string_view theme, bool compact)
	{
		if (!ImGui::GetCurrentContext())
			return;

		if (theme == "light")
			ImGui::StyleColorsLight();
		else if (theme == "classic")
			ImGui::StyleColorsClassic();
		else
			ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (theme == "midnight")
			ApplyMidnightPalette(style);

		ApplyLayout(style, compact);
	}
}
