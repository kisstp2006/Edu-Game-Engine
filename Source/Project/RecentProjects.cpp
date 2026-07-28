#include "RecentProjects.h"

#include "../Config.h"
#include "Project.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <utility>

namespace EGE
{
	namespace
	{
		constexpr int RecentProjectsFormatVersion = 1;
		constexpr std::size_t MaximumRecentProjects = 12;

		std::string PathKey(const std::filesystem::path& path)
		{
			std::string key = path.generic_string();
#ifdef _WIN32
			std::transform(
				key.begin(), key.end(), key.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
#endif
			return key;
		}
	}

	bool RecentProjects::Load(
		const std::filesystem::path& storageFile,
		std::string& error)
	{
		storageFile_ = Normalize(storageFile);
		entries_.clear();

		std::error_code fileError;
		const bool exists =
			std::filesystem::exists(storageFile_, fileError);
		if (fileError)
		{
			error =
				"Could not inspect the recent projects file: " +
				fileError.message();
			return false;
		}
		if (!exists)
			return true;
		if (!std::filesystem::is_regular_file(storageFile_, fileError))
		{
			error = fileError
				? "Could not inspect the recent projects file: " +
					fileError.message()
				: "The recent projects path is not a file.";
			return false;
		}

		std::ifstream stream(storageFile_, std::ios::binary);
		if (!stream)
		{
			error =
				"Could not read recent projects from '" +
				storageFile_.string() + "'.";
			return false;
		}

		const std::string text{
			std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>()};
		Config document(text.c_str());
		if (!document.IsValid() ||
			document.GetInt("FormatVersion", 0) !=
				RecentProjectsFormatVersion)
		{
			error = "The recent projects file has an invalid format.";
			return false;
		}

		const int count = document.GetArrayCount("Projects");
		for (int index = 0;
			index < count && entries_.size() < MaximumRecentProjects;
			++index)
		{
			const Config entry = document.GetArray("Projects", index);
			const char* pathText = entry.GetString("Path", "");
			if (!pathText || pathText[0] == '\0')
				continue;

			RecentProject recent;
			recent.projectFile = Normalize(pathText);
			const char* storedName = entry.GetString("Name", "");
			recent.name = storedName && storedName[0] != '\0'
				? storedName
				: recent.projectFile.stem().string();

			const bool duplicate = std::any_of(
				entries_.begin(), entries_.end(),
				[&recent](const RecentProject& existing)
				{
					return SamePath(
						existing.projectFile, recent.projectFile);
				});
			if (!duplicate)
				entries_.push_back(std::move(recent));
		}

		return true;
	}

	bool RecentProjects::Add(
		const Project& project,
		std::string& error)
	{
		RecentProject recent{
			project.GetName(),
			Normalize(project.GetProjectFilePath())};

		std::erase_if(
			entries_,
			[&recent](const RecentProject& existing)
			{
				return SamePath(
					existing.projectFile, recent.projectFile);
			});
		entries_.insert(entries_.begin(), std::move(recent));
		if (entries_.size() > MaximumRecentProjects)
			entries_.resize(MaximumRecentProjects);
		return Save(error);
	}

	bool RecentProjects::Remove(
		const std::filesystem::path& projectFile,
		std::string& error)
	{
		const std::size_t previousSize = entries_.size();
		std::erase_if(
			entries_,
			[&projectFile](const RecentProject& existing)
			{
				return SamePath(existing.projectFile, projectFile);
			});
		return entries_.size() == previousSize || Save(error);
	}

	const std::vector<RecentProject>& RecentProjects::GetEntries() const
	{
		return entries_;
	}

	bool RecentProjects::Save(std::string& error) const
	{
		if (storageFile_.empty())
		{
			error = "The recent projects storage path is not configured.";
			return false;
		}

		Config document;
		document.AddInt(
			"FormatVersion", RecentProjectsFormatVersion);
		document.AddArray("Projects");
		for (const RecentProject& recent : entries_)
		{
			Config entry;
			entry.AddString("Name", recent.name.c_str());
			const std::string path = recent.projectFile.string();
			entry.AddString("Path", path.c_str());
			document.AddArrayEntry(entry);
		}

		char* buffer = nullptr;
		const std::size_t size =
			document.Save(&buffer, "Edu Game Engine recent projects");
		if (!buffer || size == 0)
		{
			error = "Could not serialize the recent projects list.";
			return false;
		}

		std::error_code fileError;
		std::filesystem::create_directories(
			storageFile_.parent_path(), fileError);
		if (fileError)
		{
			delete[] buffer;
			error =
				"Could not create the recent projects directory: " +
				fileError.message();
			return false;
		}

		std::ofstream stream(
			storageFile_,
			std::ios::binary | std::ios::trunc);
		if (!stream)
		{
			delete[] buffer;
			error =
				"Could not write recent projects to '" +
				storageFile_.string() + "'.";
			return false;
		}

		stream.write(buffer, static_cast<std::streamsize>(size));
		delete[] buffer;
		if (!stream)
		{
			error = "Could not finish writing the recent projects list.";
			return false;
		}
		return true;
	}

	std::filesystem::path RecentProjects::Normalize(
		const std::filesystem::path& path)
	{
		std::error_code error;
		const std::filesystem::path absolute =
			std::filesystem::absolute(path, error);
		return error
			? path.lexically_normal()
			: absolute.lexically_normal();
	}

	bool RecentProjects::SamePath(
		const std::filesystem::path& left,
		const std::filesystem::path& right)
	{
		return PathKey(Normalize(left)) == PathKey(Normalize(right));
	}
}
