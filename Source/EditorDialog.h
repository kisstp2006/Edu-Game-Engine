#pragma once

#include <imgui.h>

namespace EGE::EditorDialog
{
	inline constexpr float BackgroundDimAlpha = 0.76f;

	inline void ApplyStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style.Colors[ImGuiCol_ModalWindowDimBg] =
			ImVec4(0.0f, 0.0f, 0.0f, BackgroundDimAlpha);
	}

	inline void Open(const char* id)
	{
		ApplyStyle();
		ImGui::OpenPopup(id);
	}

	inline bool Begin(
		const char* id,
		const ImVec2& initialSize = ImVec2(0.0f, 0.0f),
		ImGuiWindowFlags flags = 0,
		ImGuiCond sizeCondition = ImGuiCond_FirstUseEver,
		bool* open = nullptr)
	{
		ApplyStyle();

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::SetNextWindowPos(
			ImVec2(
				viewport->Pos.x + viewport->Size.x * 0.5f,
				viewport->Pos.y + viewport->Size.y * 0.5f),
			ImGuiCond_Always,
			ImVec2(0.5f, 0.5f));

		if (initialSize.x > 0.0f || initialSize.y > 0.0f)
			ImGui::SetNextWindowSize(initialSize, sizeCondition);

		const ImGuiWindowFlags dialogFlags =
			flags |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoCollapse;
		return ImGui::BeginPopupModal(id, open, dialogFlags);
	}

	inline void End()
	{
		ImGui::EndPopup();
	}
}
