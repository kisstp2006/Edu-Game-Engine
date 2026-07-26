#pragma once

#include "Project.h"

#include <filesystem>

namespace EGE
{
	class ProjectSerializer final
	{
	public:
		[[nodiscard]] static ProjectStatus Serialize(
			const Project& project,
			const std::filesystem::path& destination = {});

		[[nodiscard]] static ProjectStatus Deserialize(
			const std::filesystem::path& source,
			ProjectConfig& outputConfig);
	};
}
