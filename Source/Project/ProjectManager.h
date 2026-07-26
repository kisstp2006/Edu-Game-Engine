#pragma once

#include "Project.h"

#include <filesystem>
#include <memory>
#include <string>

namespace EGE
{
	struct ProjectOpenResult
	{
		ProjectStatus status;
		std::shared_ptr<Project> project;

		[[nodiscard]] bool Succeeded() const
		{
			return status.Succeeded() && project != nullptr;
		}

		explicit operator bool() const
		{
			return Succeeded();
		}
	};

	class ProjectManager final
	{
	public:
		// projectDirectory is the final, not-yet-existing project folder.
		[[nodiscard]] ProjectOpenResult CreateProject(
			const std::filesystem::path& projectDirectory,
			const std::string& projectName);

		// A failed open never replaces the currently active project.
		[[nodiscard]] ProjectOpenResult OpenProject(
			const std::filesystem::path& projectFilePath);

		[[nodiscard]] ProjectStatus SaveActiveProject() const;
		void CloseProject();
		void ActivateProject(std::shared_ptr<Project> project);

		[[nodiscard]] bool HasActiveProject() const;
		[[nodiscard]] std::shared_ptr<Project> GetActiveProject();
		[[nodiscard]] std::shared_ptr<const Project> GetActiveProject() const;

		[[nodiscard]] static bool IsProjectFile(
			const std::filesystem::path& path);

	private:
		std::shared_ptr<Project> activeProject_;
	};
}
