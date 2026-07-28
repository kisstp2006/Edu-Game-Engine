#include "SettingsService.h"

#include <SDL.h>
#include <utility>

namespace EGE
{
	namespace
	{
		constexpr const char* SchemaDirectory = "Settings/Schemas";
		constexpr const char* ProjectSchema = "ProjectSettings.schema.json";
		constexpr const char* EditorSchema = "EditorSettings.schema.json";
		constexpr const char* ProjectValues = "ProjectSettings.json";
		constexpr const char* EditorValues = "EditorSettings.json";
	}

	bool SettingsService::Initialize(
		const std::filesystem::path& fallbackRoot,
		const std::filesystem::path& projectRoot,
		bool loadEditorSettings,
		std::string& error)
	{
		fallbackRoot_ = fallbackRoot;
		if (!projectRoot.empty() &&
			!LoadProject(projectRoot, error))
			return false;

		if (!loadEditorSettings)
			return true;

		char* preferencePath =
			SDL_GetPrefPath("TiGames", "EduGameEngine");
		if (!preferencePath)
		{
			error = "Could not resolve the editor settings directory.";
			return false;
		}

		const std::filesystem::path valuesPath =
			std::filesystem::path(preferencePath) / EditorValues;
		SDL_free(preferencePath);

		const std::filesystem::path schemaPath =
			fallbackRoot_ / SchemaDirectory / EditorSchema;
		if (!editor_.Load(schemaPath, valuesPath, error))
			return false;

		hasEditorSettings_ = true;
		return true;
	}

	bool SettingsService::ChangeProject(
		const std::filesystem::path& projectRoot,
		std::string& error)
	{
		if (hasProjectSettings_ &&
			project_.IsDirty() &&
			!project_.Save(error))
			return false;
		return LoadProject(projectRoot, error);
	}

	void SettingsService::ClearProject()
	{
		project_ = SettingsStore();
		hasProjectSettings_ = false;
	}

	bool SettingsService::LoadProject(
		const std::filesystem::path& projectRoot,
		std::string& error)
	{
		const std::filesystem::path schemaPath =
			FindSchema(projectRoot, ProjectSchema);
		const std::filesystem::path valuesPath =
			projectRoot / "Settings" / ProjectValues;
		SettingsStore candidate;
		if (!candidate.Load(schemaPath, valuesPath, error))
			return false;

		project_ = std::move(candidate);
		hasProjectSettings_ = true;
		return true;
	}

	std::filesystem::path SettingsService::FindSchema(
		const std::filesystem::path& projectRoot,
		const char* fileName) const
	{
		const std::filesystem::path projectSchema =
			projectRoot / SchemaDirectory / fileName;
		std::error_code error;
		if (std::filesystem::is_regular_file(projectSchema, error))
			return projectSchema;

		return fallbackRoot_ / SchemaDirectory / fileName;
	}

	bool SettingsService::SaveAll(std::string& error)
	{
		if (hasProjectSettings_ &&
			project_.IsDirty() &&
			!project_.Save(error))
			return false;
		if (hasEditorSettings_ && editor_.IsDirty() &&
			!editor_.Save(error))
		{
			return false;
		}
		return true;
	}

	SettingsStore& SettingsService::Project()
	{
		return project_;
	}

	const SettingsStore& SettingsService::Project() const
	{
		return project_;
	}

	SettingsStore& SettingsService::Editor()
	{
		return editor_;
	}

	const SettingsStore& SettingsService::Editor() const
	{
		return editor_;
	}

	bool SettingsService::HasProjectSettings() const
	{
		return hasProjectSettings_;
	}

	bool SettingsService::HasEditorSettings() const
	{
		return hasEditorSettings_;
	}
}
