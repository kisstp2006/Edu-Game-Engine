#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace EGE
{
	class Project;

	struct RecentProject
	{
		std::string name;
		std::filesystem::path projectFile;
	};

	class RecentProjects final
	{
	public:
		bool Load(
			const std::filesystem::path& storageFile,
			std::string& error);
		bool Add(const Project& project, std::string& error);
		bool Remove(
			const std::filesystem::path& projectFile,
			std::string& error);

		const std::vector<RecentProject>& GetEntries() const;

	private:
		bool Save(std::string& error) const;
		static std::filesystem::path Normalize(
			const std::filesystem::path& path);
		static bool SamePath(
			const std::filesystem::path& left,
			const std::filesystem::path& right);

		std::filesystem::path storageFile_;
		std::vector<RecentProject> entries_;
	};
}
