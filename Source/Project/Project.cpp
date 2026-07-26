#include "Project.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>
#include <utility>

namespace EGE
{
	namespace
	{
		std::string ToUpperAscii(std::string value)
		{
			std::transform(value.begin(), value.end(), value.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::toupper(character));
				});
			return value;
		}

		bool IsReservedWindowsName(std::string_view name)
		{
			const size_t extensionPosition = name.find('.');
			const std::string baseName = ToUpperAscii(
				std::string(name.substr(0, extensionPosition)));

			constexpr std::array<std::string_view, 4> reservedNames = {
				"CON", "PRN", "AUX", "NUL"
			};
			if (std::find(reservedNames.begin(), reservedNames.end(), baseName) !=
				reservedNames.end())
			{
				return true;
			}

			if (baseName.size() == 4 &&
				(baseName.starts_with("COM") || baseName.starts_with("LPT")))
			{
				return baseName[3] >= '1' && baseName[3] <= '9';
			}

			return false;
		}

		std::filesystem::path NormalizeAbsolutePath(
			const std::filesystem::path& path)
		{
			std::error_code error;
			std::filesystem::path absolutePath =
				std::filesystem::absolute(path, error);
			if (error)
				return path.lexically_normal();

			return absolutePath.lexically_normal();
		}
	}

	ProjectStatus ProjectStatus::Success()
	{
		return {};
	}

	ProjectStatus ProjectStatus::Failure(
		ProjectError error, std::string message)
	{
		return {error, std::move(message)};
	}

	Project::Project(
		ProjectConfig config, std::filesystem::path projectFilePath)
		: config_(std::move(config)),
		  projectFilePath_(NormalizeAbsolutePath(projectFilePath)),
		  projectDirectory_(projectFilePath_.parent_path())
	{
	}

	ProjectConfig& Project::GetConfig()
	{
		return config_;
	}

	const ProjectConfig& Project::GetConfig() const
	{
		return config_;
	}

	const std::string& Project::GetName() const
	{
		return config_.name;
	}

	const std::filesystem::path& Project::GetProjectFilePath() const
	{
		return projectFilePath_;
	}

	const std::filesystem::path& Project::GetProjectDirectory() const
	{
		return projectDirectory_;
	}

	std::filesystem::path Project::GetAssetDirectory() const
	{
		return ResolvePath(config_.assetDirectory);
	}

	std::filesystem::path Project::GetLibraryDirectory() const
	{
		return ResolvePath(config_.libraryDirectory);
	}

	std::filesystem::path Project::GetSettingsDirectory() const
	{
		return ResolvePath(config_.settingsDirectory);
	}

	std::filesystem::path Project::GetStartScenePath() const
	{
		return ResolvePath(config_.startScene);
	}

	std::filesystem::path Project::ResolvePath(
		const std::filesystem::path& projectRelativePath) const
	{
		if (projectRelativePath.empty())
			return {};

		return (projectDirectory_ / projectRelativePath).lexically_normal();
	}

	bool IsValidProjectName(std::string_view name)
	{
		if (name.empty() || name == "." || name == ".." ||
			name.front() == ' ' || name.back() == ' ' || name.back() == '.')
		{
			return false;
		}

		constexpr std::string_view invalidCharacters = "<>:\"/\\|?*";
		for (const unsigned char character : name)
		{
			if (character < 32 ||
				invalidCharacters.find(static_cast<char>(character)) !=
					std::string_view::npos)
			{
				return false;
			}
		}

		return !IsReservedWindowsName(name);
	}

	bool IsSafeProjectRelativePath(
		const std::filesystem::path& path, bool allowEmpty)
	{
		if (path.empty())
			return allowEmpty;

		if (path.is_absolute() || path.has_root_name() ||
			path.has_root_directory())
		{
			return false;
		}

		for (const std::filesystem::path& part : path)
		{
			if (part == "." || part == ".." ||
				!IsValidProjectName(part.string()))
			{
				return false;
			}
		}

		return true;
	}

	ProjectStatus ValidateProjectConfig(const ProjectConfig& config)
	{
		if (!IsValidProjectName(config.name))
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidName,
				"The project name is empty or contains characters that are "
				"invalid in a Windows file name.");
		}

		const struct
		{
			const char* name;
			const std::filesystem::path* value;
		} requiredPaths[] = {
			{"AssetDirectory", &config.assetDirectory},
			{"LibraryDirectory", &config.libraryDirectory},
			{"SettingsDirectory", &config.settingsDirectory}
		};

		for (const auto& field : requiredPaths)
		{
			if (!IsSafeProjectRelativePath(*field.value))
			{
				return ProjectStatus::Failure(
					ProjectError::InvalidPath,
					std::string(field.name) +
						" must be a non-empty path inside the project directory.");
			}
		}

		if (!IsSafeProjectRelativePath(config.startScene, true))
		{
			return ProjectStatus::Failure(
				ProjectError::InvalidPath,
				"StartScene must be empty or point inside the project directory.");
		}

		return ProjectStatus::Success();
	}
}
