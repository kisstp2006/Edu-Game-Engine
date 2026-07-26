#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace EGE
{
	inline constexpr int ProjectFormatVersion = 1;
	inline constexpr std::string_view ProjectFileExtension = ".egeproject";

	enum class ProjectError
	{
		None,
		InvalidArgument,
		InvalidName,
		InvalidPath,
		AlreadyExists,
		NotFound,
		WrongFileType,
		InvalidFormat,
		UnsupportedVersion,
		FileSystemError,
		NoActiveProject
	};

	struct ProjectStatus
	{
		ProjectError error = ProjectError::None;
		std::string message;

		[[nodiscard]] bool Succeeded() const
		{
			return error == ProjectError::None;
		}

		explicit operator bool() const
		{
			return Succeeded();
		}

		static ProjectStatus Success();
		static ProjectStatus Failure(ProjectError error, std::string message);
	};

	struct ProjectConfig
	{
		std::string name = "Untitled";
		std::filesystem::path assetDirectory = "Assets";
		std::filesystem::path libraryDirectory = "Library";
		std::filesystem::path settingsDirectory = "Settings";
		std::filesystem::path startScene;
	};

	class Project final
	{
	public:
		Project(ProjectConfig config, std::filesystem::path projectFilePath);

		[[nodiscard]] ProjectConfig& GetConfig();
		[[nodiscard]] const ProjectConfig& GetConfig() const;

		[[nodiscard]] const std::string& GetName() const;
		[[nodiscard]] const std::filesystem::path& GetProjectFilePath() const;
		[[nodiscard]] const std::filesystem::path& GetProjectDirectory() const;

		[[nodiscard]] std::filesystem::path GetAssetDirectory() const;
		[[nodiscard]] std::filesystem::path GetLibraryDirectory() const;
		[[nodiscard]] std::filesystem::path GetSettingsDirectory() const;
		[[nodiscard]] std::filesystem::path GetStartScenePath() const;
		[[nodiscard]] std::filesystem::path ResolvePath(
			const std::filesystem::path& projectRelativePath) const;

	private:
		ProjectConfig config_;
		std::filesystem::path projectFilePath_;
		std::filesystem::path projectDirectory_;
	};

	[[nodiscard]] bool IsValidProjectName(std::string_view name);
	[[nodiscard]] bool IsSafeProjectRelativePath(
		const std::filesystem::path& path, bool allowEmpty = false);
	[[nodiscard]] ProjectStatus ValidateProjectConfig(const ProjectConfig& config);
}
