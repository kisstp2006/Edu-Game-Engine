#include "ImportModelDlg.h"
#include "EditorDialog.h"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

ImportModelDlg::ImportModelDlg()
	: popupName_(
		"Model import settings##" +
		std::to_string(reinterpret_cast<std::size_t>(this)))
{
}

void ImportModelDlg::Open(const std::string& file)
{
	file_ = file;
	selection_ = false;
	openRequested_ = true;
	settings_.Clear();

	std::string assetName =
		std::filesystem::path(file).stem().string();
	EGE::ImportSetting& name = settings_.AddString(
		"assetName", "Asset name", std::move(assetName));
	name.group = "GENERAL";
	name.tooltip = "Name of the imported model resource.";

	EGE::ImportSetting& scale = settings_.AddVector3(
		"scale", "Scale", float3::one);
	scale.group = "TRANSFORM";
	scale.tooltip =
		"Per-axis scale applied to vertices, nodes and translations.";
	scale.minimum = 0.0001;
	scale.maximum = 10000.0;
	scale.step = 0.01;

	EGE::ImportSetting& materials = settings_.AddBoolean(
		"importMaterials", "Import materials", true);
	materials.group = "CONTENT";
	materials.tooltip =
		"Imports materials and referenced textures when supported.";

	EGE::ImportSetting& normals = settings_.AddBoolean(
		"generateNormals", "Generate normals", true);
	normals.group = "GEOMETRY";
	normals.tooltip =
		"Generates smooth normals when the source does not contain them.";
	settings_.AddBoolean(
		"generateTangents", "Generate tangents", true).tooltip =
		"Generates tangent space required by normal mapping.";
	settings_.AddBoolean(
		"weldVertices", "Weld vertices", true).tooltip =
		"Merges identical vertices during compatible model imports.";
	settings_.AddBoolean(
		"optimizeMeshes", "Optimize meshes", true).tooltip =
		"Optimizes mesh and node ordering for rendering.";
	settings_.AddBoolean(
		"flipUVs", "Flip UV vertically", false).tooltip =
		"Flips the V texture coordinate during import.";
	EGE::ImportSetting& coordinates = settings_.AddBoolean(
		"convertGlTfCoordinates",
		"Convert glTF coordinates",
		true);
	coordinates.group = "TRANSFORM";
	coordinates.tooltip =
		"Converts GLTF and GLB assets to the engine coordinate system.";
}

void ImportModelDlg::Display()
{
	if (openRequested_)
	{
		EGE::EditorDialog::Open(popupName_.c_str());
		openRequested_ = false;
	}

	if (!EGE::EditorDialog::Begin(
			popupName_.c_str(), ImVec2(530.0f, 470.0f)))
	{
		return;
	}

	ImGui::TextDisabled("%s", file_.c_str());
	ImGui::Separator();
	if (ImGui::BeginChild(
			"##ModelImportFields", ImVec2(0.0f, -36.0f), false))
	{
		EGE::DrawImportSettings(settings_);
	}
	ImGui::EndChild();

	const EGE::ModelImportOptions options = GetOptions();
	const bool valid =
		!options.assetName.empty() &&
		options.scale.x > 0.0f &&
		options.scale.y > 0.0f &&
		options.scale.z > 0.0f;
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

void ImportModelDlg::ClearSelection()
{
	file_.clear();
	settings_.Clear();
	selection_ = false;
	openRequested_ = false;
}

bool ImportModelDlg::HasSelection() const
{
	return selection_;
}

const std::string& ImportModelDlg::GetFile() const
{
	return file_;
}

EGE::ModelImportOptions ImportModelDlg::GetOptions() const
{
	EGE::ModelImportOptions options;
	options.assetName = settings_.GetString("assetName");
	options.scale = settings_.GetVector3("scale", float3::one);
	options.importMaterials =
		settings_.GetBoolean("importMaterials", true);
	options.generateNormals =
		settings_.GetBoolean("generateNormals", true);
	options.generateTangents =
		settings_.GetBoolean("generateTangents", true);
	options.weldVertices = settings_.GetBoolean("weldVertices", true);
	options.optimizeMeshes =
		settings_.GetBoolean("optimizeMeshes", true);
	options.flipUVs = settings_.GetBoolean("flipUVs", false);
	options.convertGlTfCoordinates =
		settings_.GetBoolean("convertGlTfCoordinates", true);
	return options;
}
