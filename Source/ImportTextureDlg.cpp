#include "ImportTextureDlg.h"
#include "EditorDialog.h"

#include <imgui.h>

ImportTexturesDlg::ImportTexturesDlg()
	: popupName_(
		"Texture import settings##" +
		std::to_string(reinterpret_cast<std::size_t>(this)))
{
}

void ImportTexturesDlg::Open(const std::string& file)
{
	file_ = file;
	selection_ = false;
	openRequested_ = true;
	settings_.Clear();

	EGE::ImportSetting& mipmaps = settings_.AddBoolean(
		"mipmaps", "Generate mipmaps", true);
	mipmaps.group = "SAMPLING";
	mipmaps.tooltip =
		"Builds a complete mip chain for minified rendering.";

	EGE::ImportSetting& colorSpace = settings_.AddEnumeration(
		"colorSpace",
		"Color space",
		1,
		{{"Linear", 0}, {"sRGB", 1}});
	colorSpace.tooltip =
		"Use sRGB for color textures and Linear for data textures.";

	EGE::ImportSetting& cubemap = settings_.AddBoolean(
		"cubemap", "Equirectangular cubemap", false);
	cubemap.group = "OUTPUT";
	cubemap.tooltip =
		"Converts a 2:1 equirectangular image into a cubemap.";

	EGE::ImportSetting& faceSize = settings_.AddInteger(
		"cubemapFaceSize", "Cubemap face size", 512);
	faceSize.minimum = 16.0;
	faceSize.maximum = 4096.0;
	faceSize.enabledWhen = [](const EGE::ImportSettings& values)
	{
		return values.GetBoolean("cubemap");
	};

	EGE::ImportSetting& maximumSize = settings_.AddVector2(
		"maximumSize", "Maximum size", float2::zero);
	maximumSize.tooltip =
		"Maximum width and height. Zero keeps the original dimension.";
	maximumSize.minimum = 0.0;
	maximumSize.maximum = 16384.0;
	maximumSize.step = 1.0;
	maximumSize.enabledWhen = [](const EGE::ImportSettings& values)
	{
		return !values.GetBoolean("cubemap");
	};

	EGE::ImportSetting& tint = settings_.AddColor(
		"colorMultiplier", "Color multiplier", float4::one);
	tint.group = "PROCESSING";
	tint.tooltip =
		"Multiplies imported RGBA pixels before mip generation.";
}

void ImportTexturesDlg::Display()
{
	if (openRequested_)
	{
		EGE::EditorDialog::Open(popupName_.c_str());
		openRequested_ = false;
	}

	if (!EGE::EditorDialog::Begin(
			popupName_.c_str(), ImVec2(530.0f, 410.0f)))
	{
		return;
	}

	ImGui::TextDisabled("%s", file_.c_str());
	ImGui::Separator();
	if (ImGui::BeginChild(
			"##TextureImportFields", ImVec2(0.0f, -36.0f), false))
	{
		EGE::DrawImportSettings(settings_);
	}
	ImGui::EndChild();

	if (ImGui::Button("Import", ImVec2(90.0f, 0.0f)))
	{
		selection_ = true;
		ImGui::CloseCurrentPopup();
	}
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

void ImportTexturesDlg::ClearSelection()
{
	file_.clear();
	settings_.Clear();
	selection_ = false;
	openRequested_ = false;
}

bool ImportTexturesDlg::HasSelection() const
{
	return selection_;
}

const std::string& ImportTexturesDlg::GetFile() const
{
	return file_;
}

EGE::TextureImportOptions ImportTexturesDlg::GetOptions() const
{
	EGE::TextureImportOptions options;
	options.generateMipmaps = settings_.GetBoolean("mipmaps", true);
	options.sRgb = settings_.GetInteger("colorSpace", 1) == 1;
	options.convertToCubemap = settings_.GetBoolean("cubemap");
	options.cubemapFaceSize = static_cast<std::uint32_t>(
		settings_.GetInteger("cubemapFaceSize", 512));
	options.maximumSize =
		settings_.GetVector2("maximumSize", float2::zero);
	options.colorMultiplier =
		settings_.GetColor("colorMultiplier", float4::one);
	return options;
}
