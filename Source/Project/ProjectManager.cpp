#include "ProjectManager.h"

#include "ProjectSerializer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <system_error>
#include <utility>

namespace EGE
{
	namespace
	{
		class TemporaryDirectory final
		{
		public:
			explicit TemporaryDirectory(std::filesystem::path path)
				: path_(std::move(path))
			{
			}

			~TemporaryDirectory()
			{
				if (!keep_)
				Remove();
			}

			void Keep()
			{
				keep_ = true;
			}

		private:
			void Remove()
			{
				std::error_code ignored;
				std::filesystem::remove_all(path_, ignored);
			}

			std::filesystem::path path_;
			bool keep_ = false;
		};

		std::filesystem::path NormalizeAbsolutePath(
			const std::filesystem::path& path, std::error_code& error)
		{
			std::filesystem::path absolutePath =
				std::filesystem::absolute(path, error);
			if (error)
				return {};

			return absolutePath.lexically_normal();
		}

		std::filesystem::path MakeStagingDirectory(
			const std::filesystem::path& projectDirectory)
		{
			const auto suffix =
				std::chrono::steady_clock::now().time_since_epoch().count();
			std::filesystem::path staging = projectDirectory;
			staging += ".creating-" + std::to_string(suffix);
			return staging;
		}

		ProjectStatus CreateProjectDirectories(
			const std::filesystem::path& projectDirectory,
			const ProjectConfig& config)
		{
			const std::filesystem::path assets =
				projectDirectory / config.assetDirectory;
			const std::filesystem::path library =
				projectDirectory / config.libraryDirectory;

			const std::array<std::filesystem::path, 19> directories = {
				assets,
				assets / "Animation",
				assets / "Audio",
				assets / "Fonts",
				assets / "Models",
				assets / "Scenes",
				assets / "Scripts",
				assets / "Shaders",
				assets / "Textures",
				library,
				library / "Animations",
				library / "Audio",
				library / "Materials",
				library / "Meshes",
				library / "Models",
				library / "Scenes",
				library / "StateMachines",
				library / "Textures",
				projectDirectory / config.settingsDirectory
			};

			for (const std::filesystem::path& directory : directories)
			{
				std::error_code error;
				std::filesystem::create_directories(directory, error);
				if (error)
				{
					return ProjectStatus::Failure(
						ProjectError::FileSystemError,
						"Could not create directory '" + directory.string() +
							"': " + error.message());
				}
			}

			return ProjectStatus::Success();
		}

		ProjectOpenResult Failure(ProjectStatus status)
		{
			return {std::move(status), nullptr};
		}
	}

	ProjectOpenResult ProjectManager::CreateProject(
		const std::filesystem::path& projectDirectory,
		const std::string& projectName)
	{
		if (projectDirectory.empty())
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::InvalidArgument,
				"The project directory is empty."));
		}
		if (!IsValidProjectName(projectName))
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::InvalidName,
				"The project name is empty or is not a valid Windows file name."));
		}

		std::error_code error;
		const std::filesystem::path normalizedDirectory =
			NormalizeAbsolutePath(projectDirectory, error);
		if (error)
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::InvalidPath,
				"Could not resolve project directory '" +
					projectDirectory.string() + "': " + error.message()));
		}

		if (std::filesystem::exists(normalizedDirectory, error))
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::AlreadyExists,
				"Project directory '" + normalizedDirectory.string() +
					"' already exists."));
		}
		if (error)
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not inspect project directory '" +
					normalizedDirectory.string() + "': " + error.message()));
		}

		const std::filesystem::path parent =
			normalizedDirectory.parent_path();
		if (!std::filesystem::is_directory(parent, error))
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::NotFound,
				"Parent directory '" + parent.string() +
					"' does not exist or is not a directory."));
		}

		const std::filesystem::path stagingDirectory =
			MakeStagingDirectory(normalizedDirectory);
		if (!std::filesystem::create_directory(stagingDirectory, error))
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not create temporary project directory '" +
					stagingDirectory.string() + "': " + error.message()));
		}
		TemporaryDirectory stagingCleanup(stagingDirectory);

		ProjectConfig config;
		config.name = projectName;
		ProjectStatus status =
			CreateProjectDirectories(stagingDirectory, config);
		if (!status)
			return Failure(std::move(status));

		const std::string projectFileName =
			projectName + std::string(ProjectFileExtension);
		Project stagingProject(
			config, stagingDirectory / projectFileName);
		status = ProjectSerializer::Serialize(stagingProject);
		if (!status)
			return Failure(std::move(status));

		std::filesystem::rename(
			stagingDirectory, normalizedDirectory, error);
		if (error)
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not finish project creation: " + error.message()));
		}
		stagingCleanup.Keep();

		auto project = std::make_shared<Project>(
			std::move(config), normalizedDirectory / projectFileName);
		activeProject_ = project;
		return {ProjectStatus::Success(), std::move(project)};
	}

	ProjectOpenResult ProjectManager::OpenProject(
		const std::filesystem::path& projectFilePath)
	{
		if (!IsProjectFile(projectFilePath))
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::WrongFileType,
				"Expected a " + std::string(ProjectFileExtension) +
					" project file."));
		}

		std::error_code error;
		const std::filesystem::path normalizedPath =
			NormalizeAbsolutePath(projectFilePath, error);
		if (error)
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::InvalidPath,
				"Could not resolve project file '" +
					projectFilePath.string() + "': " + error.message()));
		}

		const bool isRegularFile =
			std::filesystem::is_regular_file(normalizedPath, error);
		if (error)
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::FileSystemError,
				"Could not inspect project file '" +
					normalizedPath.string() + "': " + error.message()));
		}
		if (!isRegularFile)
		{
			return Failure(ProjectStatus::Failure(
				ProjectError::NotFound,
				"Project file '" + normalizedPath.string() +
					"' does not exist."));
		}

		ProjectConfig config;
		ProjectStatus status =
			ProjectSerializer::Deserialize(normalizedPath, config);
		if (!status)
			return Failure(std::move(status));

		auto project =
			std::make_shared<Project>(std::move(config), normalizedPath);

		// Commit only after the candidate was fully read and validated.
		activeProject_ = project;
		return {ProjectStatus::Success(), std::move(project)};
	}

	ProjectStatus ProjectManager::SaveActiveProject() const
	{
		if (!activeProject_)
		{
			return ProjectStatus::Failure(
				ProjectError::NoActiveProject,
				"There is no active project to save.");
		}

		return ProjectSerializer::Serialize(*activeProject_);
	}

	void ProjectManager::CloseProject()
	{
		activeProject_.reset();
	}

	void ProjectManager::ActivateProject(std::shared_ptr<Project> project)
	{
		activeProject_ = std::move(project);
	}

	bool ProjectManager::HasActiveProject() const
	{
		return activeProject_ != nullptr;
	}

	std::shared_ptr<Project> ProjectManager::GetActiveProject()
	{
		return activeProject_;
	}

	std::shared_ptr<const Project> ProjectManager::GetActiveProject() const
	{
		return activeProject_;
	}

	bool ProjectManager::IsProjectFile(
		const std::filesystem::path& path)
	{
		std::string extension = path.extension().string();
		std::transform(
			extension.begin(), extension.end(), extension.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return extension == ProjectFileExtension;
	}
}
