#ifndef EGE_SCRIPT_RESOURCE_H
#define EGE_SCRIPT_RESOURCE_H

#include <filesystem>
#include <string>

namespace EGE
{
	struct ScriptResourceInfo
	{
		std::string assetId;
		std::filesystem::path sourcePath;

		[[nodiscard]] explicit operator bool() const
		{
			return !assetId.empty();
		}
	};

	class ScriptResource final
	{
	public:
		[[nodiscard]] static std::string CreateAssetId();
		[[nodiscard]] static bool IsAssetId(const std::string& value);
		[[nodiscard]] static ScriptResourceInfo Read(
			const std::filesystem::path& path);
		[[nodiscard]] static ScriptResourceInfo ReadOrCreate(
			const std::filesystem::path& path,
			std::string& error);
		[[nodiscard]] static std::string AddAssetId(
			const std::string& source,
			const std::string& assetId);
	};
}

#endif
