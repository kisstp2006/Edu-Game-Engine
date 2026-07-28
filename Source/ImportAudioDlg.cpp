#include "ImportAudioDlg.h"
#include "EditorDialog.h"

#include <imgui.h>

#include <filesystem>

ImportAudioDlg::ImportAudioDlg()
	: popupName_(
		"Audio import settings##" +
		std::to_string(reinterpret_cast<std::size_t>(this)))
{
}

void ImportAudioDlg::Open(const std::string& file)
{
	file_ = file;
	selection_ = false;
	openRequested_ = true;
	settings_.Clear();

	EGE::ImportSetting& name = settings_.AddString(
		"assetName",
		"Asset name",
		std::filesystem::path(file).stem().string());
	name.group = "GENERAL";

	EGE::ImportSetting& mode = settings_.AddEnumeration(
		"mode",
		"Load mode",
		static_cast<std::int64_t>(EGE::AudioImportMode::Automatic),
		{
			{"Automatic", static_cast<std::int64_t>(
				EGE::AudioImportMode::Automatic)},
			{"Sample (decoded)", static_cast<std::int64_t>(
				EGE::AudioImportMode::Sample)},
			{"Stream", static_cast<std::int64_t>(
				EGE::AudioImportMode::Stream)}
		});
	mode.group = "PLAYBACK";
	mode.tooltip =
		"Samples use memory for low latency; streams suit long audio.";
	settings_.AddBoolean("loop", "Loop by default", false);

	EGE::ImportSetting& volume =
		settings_.AddFloat("volume", "Default volume", 1.0);
	volume.minimum = 0.0;
	volume.maximum = 2.0;
	volume.step = 0.01;

	EGE::ImportSetting& pitch =
		settings_.AddFloat("pitch", "Default pitch", 1.0);
	pitch.minimum = 0.1;
	pitch.maximum = 4.0;
	pitch.step = 0.01;

	EGE::ImportSetting& spatial =
		settings_.AddBoolean("spatial", "Spatial audio", false);
	spatial.group = "SPATIAL";
	EGE::ImportSetting& distances = settings_.AddVector2(
		"distanceRange", "Distance range", float2(1.0f, 100.0f));
	distances.tooltip = "Minimum and maximum 3D attenuation distance.";
	distances.minimum = 0.0;
	distances.maximum = 100000.0;
	distances.step = 0.1;
	distances.enabledWhen = [](const EGE::ImportSettings& values)
	{
		return values.GetBoolean("spatial");
	};
}

void ImportAudioDlg::Display()
{
	if (openRequested_)
	{
		EGE::EditorDialog::Open(popupName_.c_str());
		openRequested_ = false;
	}

	if (!EGE::EditorDialog::Begin(
			popupName_.c_str(), ImVec2(530.0f, 440.0f)))
	{
		return;
	}

	ImGui::TextDisabled("%s", file_.c_str());
	ImGui::Separator();
	if (ImGui::BeginChild(
			"##AudioImportFields", ImVec2(0.0f, -36.0f), false))
	{
		EGE::DrawImportSettings(settings_);
	}
	ImGui::EndChild();

	const EGE::AudioImportOptions options = GetOptions();
	const bool valid =
		!options.assetName.empty() &&
		options.volume >= 0.0f &&
		options.pitch > 0.0f &&
		options.distanceRange.x >= 0.0f &&
		options.distanceRange.y >= options.distanceRange.x;
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

void ImportAudioDlg::ClearSelection()
{
	file_.clear();
	settings_.Clear();
	selection_ = false;
	openRequested_ = false;
}

bool ImportAudioDlg::HasSelection() const
{
	return selection_;
}

const std::string& ImportAudioDlg::GetFile() const
{
	return file_;
}

EGE::AudioImportOptions ImportAudioDlg::GetOptions() const
{
	EGE::AudioImportOptions options;
	options.assetName = settings_.GetString("assetName");
	options.mode = static_cast<EGE::AudioImportMode>(
		settings_.GetInteger(
			"mode",
			static_cast<std::int64_t>(
				EGE::AudioImportMode::Automatic)));
	options.loop = settings_.GetBoolean("loop");
	options.volume =
		static_cast<float>(settings_.GetFloat("volume", 1.0));
	options.pitch =
		static_cast<float>(settings_.GetFloat("pitch", 1.0));
	options.spatial = settings_.GetBoolean("spatial");
	options.distanceRange = settings_.GetVector2(
		"distanceRange", float2(1.0f, 100.0f));
	return options;
}
