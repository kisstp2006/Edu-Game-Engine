#include "ImportAnimationDlg.h"
#include "EditorDialog.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace
{
	bool DrawClipList(
		const EGE::ImportSetting&,
		std::vector<EGE::AnimationClipImportRange>& clips)
	{
		bool changed = false;
		std::size_t removeIndex =
			(std::numeric_limits<std::size_t>::max)();
		for (std::size_t index = 0; index < clips.size(); ++index)
		{
			EGE::AnimationClipImportRange& clip = clips[index];
			ImGui::PushID(static_cast<int>(index));
			if (ImGui::BeginChild(
					"##Clip",
					ImVec2(0.0f, 94.0f),
					true,
					ImGuiWindowFlags_NoScrollbar))
			{
				ImGui::SetNextItemWidth(-30.0f);
				changed |= ImGui::InputText("##Name", &clip.name);
				ImGui::SameLine();
				if (ImGui::SmallButton("X"))
					removeIndex = index;

				int first = static_cast<int>(std::min<std::uint32_t>(
					clip.firstFrame,
					static_cast<std::uint32_t>((std::numeric_limits<int>::max)())));
				int last = clip.lastFrame ==
					(std::numeric_limits<std::uint32_t>::max)()
						? -1
						: static_cast<int>(std::min<std::uint32_t>(
							clip.lastFrame,
							static_cast<std::uint32_t>((std::numeric_limits<int>::max)())));
				ImGui::SetNextItemWidth(
					(ImGui::GetContentRegionAvail().x -
						ImGui::GetStyle().ItemSpacing.x) * 0.5f);
				if (ImGui::InputInt("##First", &first))
				{
					clip.firstFrame =
						static_cast<std::uint32_t>(std::max(0, first));
					changed = true;
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputInt("##Last", &last))
				{
					clip.lastFrame = last < 0
						? (std::numeric_limits<std::uint32_t>::max)()
						: static_cast<std::uint32_t>(
							std::max(first, last));
					changed = true;
				}
				ImGui::TextDisabled("First frame");
				ImGui::SameLine(
					ImGui::GetWindowContentRegionMax().x * 0.52f);
				ImGui::TextDisabled("Last frame (-1 = end)");
			}
			ImGui::EndChild();
			ImGui::PopID();
		}

		if (removeIndex < clips.size())
		{
			clips.erase(
				clips.begin() +
				static_cast<std::ptrdiff_t>(removeIndex));
			changed = true;
		}
		if (ImGui::Button("+ Add clip"))
		{
			EGE::AnimationClipImportRange clip;
			clip.name = "Clip " + std::to_string(clips.size() + 1);
			clips.push_back(std::move(clip));
			changed = true;
		}
		return changed;
	}
}

ImportAnimationDlg::ImportAnimationDlg()
{
	EGE::ImportCustomEditorRegistry::Get().Register<
		std::vector<EGE::AnimationClipImportRange>>(
		ClipListType, DrawClipList);
}

void ImportAnimationDlg::Open(
	const std::string& file,
	const std::string& name)
{
	file_ = file;
	selection_ = false;
	openRequested_ = true;
	settings_.Clear();

	EGE::ImportSetting& scale = settings_.AddVector3(
		"scale", "Translation scale", float3::one);
	scale.group = "TRANSFORM";
	scale.tooltip =
		"Per-axis scale applied to animation position keys.";
	scale.minimum = 0.0001;
	scale.maximum = 10000.0;
	scale.step = 0.01;

	EGE::ImportSetting& coordinates = settings_.AddBoolean(
		"convertGlTfCoordinates",
		"Convert glTF coordinates",
		true);
	coordinates.group = "TRANSFORM";
	coordinates.tooltip =
		"Uses the same GLTF and GLB coordinate conversion as model import.";

	EGE::ImportSetting& morphTargets = settings_.AddBoolean(
		"importMorphTargets", "Import morph targets", true);
	morphTargets.group = "CONTENT";
	morphTargets.tooltip =
		"Imports animated blend-shape weight channels.";

	EGE::AnimationClipImportRange defaultClip;
	defaultClip.name = name.empty() ? "Animation" : name;
	EGE::ImportSetting& clips = settings_.AddCustom(
		"clips",
		"Clips",
		ClipListType,
		std::vector<EGE::AnimationClipImportRange>{defaultClip});
	clips.group = "CLIPS";
	clips.tooltip =
		"One resource is generated for every configured frame range.";
}

void ImportAnimationDlg::Display()
{
	if (openRequested_)
	{
		EGE::EditorDialog::Open("Animation import settings");
		openRequested_ = false;
	}

	if (!EGE::EditorDialog::Begin(
			"Animation import settings",
			ImVec2(620.0f, 560.0f)))
	{
		return;
	}

	ImGui::TextDisabled("%s", file_.c_str());
	ImGui::Separator();
	if (ImGui::BeginChild(
			"##AnimationImportFields", ImVec2(0.0f, -36.0f), false))
	{
		EGE::DrawImportSettings(settings_);
	}
	ImGui::EndChild();

	const EGE::AnimationImportOptions options = GetOptions();
	const bool valid =
		!options.clips.empty() &&
		std::all_of(
			options.clips.begin(), options.clips.end(),
			[](const EGE::AnimationClipImportRange& clip)
			{
				return !clip.name.empty() &&
					(clip.lastFrame ==
							(std::numeric_limits<std::uint32_t>::max)() ||
					 clip.lastFrame >= clip.firstFrame);
			});
	if (!valid)
		ImGui::PushStyleVar(
			ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.45f);
	if (ImGui::Button("Import", ImVec2(90.0f, 0.0f)) && valid)
	{
		selection_ = true;
		ImGui::CloseCurrentPopup();
	}
	if (!valid)
		ImGui::PopStyleVar();
	ImGui::SameLine();
	if (ImGui::Button("Reset", ImVec2(80.0f, 0.0f)))
		settings_.Reset();
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(90.0f, 0.0f)) ||
		ImGui::IsKeyPressed(
			ImGui::GetKeyIndex(ImGuiKey_Escape), false))
	{
		ClearSelection();
		ImGui::CloseCurrentPopup();
	}
	EGE::EditorDialog::End();
}

void ImportAnimationDlg::ClearSelection()
{
	file_.clear();
	settings_.Clear();
	selection_ = false;
	openRequested_ = false;
}

bool ImportAnimationDlg::HasSelection() const
{
	return selection_;
}

const std::string& ImportAnimationDlg::GetFile() const
{
	return file_;
}

EGE::AnimationImportOptions ImportAnimationDlg::GetOptions() const
{
	EGE::AnimationImportOptions options;
	options.scale = settings_.GetVector3("scale", float3::one);
	options.importMorphTargets =
		settings_.GetBoolean("importMorphTargets", true);
	options.convertGlTfCoordinates =
		settings_.GetBoolean("convertGlTfCoordinates", true);
	if (const auto* clips =
			settings_.GetCustom<
				std::vector<EGE::AnimationClipImportRange>>(
				"clips", ClipListType))
	{
		options.clips = *clips;
	}
	return options;
}
