#include "ProjectSerializer.h"

#include "../parson.h"

#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace EGE
{
	namespace
	{
		using JsonValuePtr = std::unique_ptr<JSON_Value, decltype(&json_value_free)>;
		using JsonStringPtr =
			std::unique_ptr<char, decltype(&json_free_serialized_string)>;

		ProjectStatus ReadRequiredString(
			const JSON_Object* object, const char* fieldName,
			std::filesystem::path& output)
		{
			const JSON_Value* value = json_object_get_value(object, fieldName);
			if (!value || json_value_get_type(value) != JSONString)
			{
				return ProjectStatus::Failure(
					ProjectError::InvalidFormat,
					std::string("Project field '") + fieldName +
						"' is missing or is not a string.");
			}

			output = json_value_get_string(value);
			return ProjectStatus::Success();
		}

		ProjectStatus ReadRequiredString(
			const JSON_Object* object, const char* fieldName,
			std::string& output)
		{
			const JSON_Value* value = json_object_get_value(object, fieldName);
			if (!value || json_value_get_type(value) != JSONString)
			{
				return ProjectStatus::Failure(
					ProjectError::InvalidFormat,
					std::string("Project field '") + fieldName +
						"' is missing or is not a string.");
			}

			output = json_value_get_string(value);
			return ProjectStatus::Success();
		}

		std::filesystem::path MakeTemporaryFilePath(
			const std::filesystem::path& destination)
		{
			const auto suffix =
				std::chrono::steady_clock::now().time_since_epoch().count();
			std::filesystem::path temporary = destination;
			temporary += ".tmp-" + std::to_string(suffix);
			return temporary;
		}

		ProjectStatus ReplaceFile(
			const std::filesystem::path& temporary,
			const std::filesystem::path& destination)
		{
#if defined(_WIN32)
			if (MoveFileExW(
					temporary.c_str(), destination.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				return ProjectStatus::Success();
			}

			const std::error_code error(
				static_cast<int>(GetLastError()), std::system_category());
#else
			std::error_code error;
			std::filesystem::rename(temporary, destination, error);
			if (!error)
				return ProjectStatus::Success();
#endif

			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not replace project file '" + destination.string() +
					"': " + error.message());
		}
	}

	ProjectStatus ProjectSerializer::Serialize(
		const Project& project, const std::filesystem::path& destination)
	{
		const ProjectStatus validation =
			ValidateProjectConfig(project.GetConfig());
		if (!validation)
			return validation;

		const std::filesystem::path outputPath =
			destination.empty() ? project.GetProjectFilePath() : destination;
		if (outputPath.empty())
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidPath,
				"A destination project file was not provided.");
		}

		JsonValuePtr rootValue(json_value_init_object(), json_value_free);
		JsonValuePtr projectValue(json_value_init_object(), json_value_free);
		if (!rootValue || !projectValue)
		{
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not allocate the project JSON document.");
		}

		JSON_Object* root = json_value_get_object(rootValue.get());
		JSON_Object* projectNode = json_value_get_object(projectValue.get());
		const ProjectConfig& config = project.GetConfig();

		if (json_object_set_number(
				root, "FormatVersion", ProjectFormatVersion) != JSONSuccess ||
			json_object_set_string(
				projectNode, "Name", config.name.c_str()) != JSONSuccess ||
			json_object_set_string(
				projectNode, "AssetDirectory",
				config.assetDirectory.generic_string().c_str()) != JSONSuccess ||
			json_object_set_string(
				projectNode, "LibraryDirectory",
				config.libraryDirectory.generic_string().c_str()) != JSONSuccess ||
			json_object_set_string(
				projectNode, "SettingsDirectory",
				config.settingsDirectory.generic_string().c_str()) != JSONSuccess ||
			json_object_set_string(
				projectNode, "StartScene",
				config.startScene.generic_string().c_str()) != JSONSuccess)
		{
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not build the project JSON document.");
		}

		JSON_Value* rawProjectValue = projectValue.release();
		if (json_object_set_value(
				root, "Project", rawProjectValue) != JSONSuccess)
		{
			json_value_free(rawProjectValue);
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not attach the Project object to the JSON document.");
		}

		JsonStringPtr serialized(
			json_serialize_to_string_pretty(rootValue.get()),
			json_free_serialized_string);
		if (!serialized)
		{
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not serialize the project JSON document.");
		}

		const std::filesystem::path temporaryPath =
			MakeTemporaryFilePath(outputPath);
		std::ofstream output(
			temporaryPath, std::ios::binary | std::ios::trunc);
		if (!output.is_open())
		{
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not open temporary project file '" +
					temporaryPath.string() + "'.");
		}

		output << serialized.get() << '\n';
		output.flush();
		if (!output.good())
		{
			output.close();
			std::error_code ignored;
			std::filesystem::remove(temporaryPath, ignored);
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not write project file '" + outputPath.string() + "'.");
		}
		output.close();

		return ReplaceFile(temporaryPath, outputPath);
	}

	ProjectStatus ProjectSerializer::Deserialize(
		const std::filesystem::path& source, ProjectConfig& outputConfig)
	{
		std::ifstream input(source, std::ios::binary);
		if (!input.is_open())
		{
			return ProjectStatus::Failure(
				ProjectError::NotFound,
				"Could not open project file '" + source.string() + "'.");
		}

		std::ostringstream contents;
		contents << input.rdbuf();
		if (!input.good() && !input.eof())
		{
			return ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not read project file '" + source.string() + "'.");
		}

		JsonValuePtr rootValue(
			json_parse_string(contents.str().c_str()), json_value_free);
		if (!rootValue ||
			json_value_get_type(rootValue.get()) != JSONObject)
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidFormat,
				"The project file does not contain a valid JSON object.");
		}

		const JSON_Object* root = json_value_get_object(rootValue.get());
		const JSON_Value* versionValue =
			json_object_get_value(root, "FormatVersion");
		if (!versionValue ||
			json_value_get_type(versionValue) != JSONNumber)
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidFormat,
				"The project file has no numeric FormatVersion.");
		}

		const double serializedVersion =
			json_value_get_number(versionValue);
		const int formatVersion =
			static_cast<int>(serializedVersion);
		if (serializedVersion != static_cast<double>(formatVersion))
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidFormat,
				"FormatVersion must be an integer.");
		}
		if (formatVersion != ProjectFormatVersion)
		{
			return ProjectStatus::Failure(
				ProjectError::UnsupportedVersion,
				"Unsupported project format version " +
					std::to_string(formatVersion) + ".");
		}

		const JSON_Object* projectNode =
			json_object_get_object(root, "Project");
		if (!projectNode)
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidFormat,
				"The project file has no Project object.");
		}

		ProjectConfig candidate;
		ProjectStatus status =
			ReadRequiredString(projectNode, "Name", candidate.name);
		if (!status)
			return status;
		status = ReadRequiredString(
			projectNode, "AssetDirectory", candidate.assetDirectory);
		if (!status)
			return status;
		status = ReadRequiredString(
			projectNode, "LibraryDirectory", candidate.libraryDirectory);
		if (!status)
			return status;
		status = ReadRequiredString(
			projectNode, "SettingsDirectory", candidate.settingsDirectory);
		if (!status)
			return status;
		status = ReadRequiredString(
			projectNode, "StartScene", candidate.startScene);
		if (!status)
			return status;

		status = ValidateProjectConfig(candidate);
		if (!status)
			return status;

		outputConfig = std::move(candidate);
		return ProjectStatus::Success();
	}
}
