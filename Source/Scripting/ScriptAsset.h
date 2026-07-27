#ifndef EGE_SCRIPT_ASSET_H
#define EGE_SCRIPT_ASSET_H

#include <filesystem>
#include <string>

namespace EGE
{
	struct ScriptAssetCreationResult
	{
		std::string sourcePath;
		std::string className;
		std::string error;

		[[nodiscard]] explicit operator bool() const
		{
			return !sourcePath.empty();
		}
	};

	class ScriptAsset final
	{
	public:
		[[nodiscard]] static ScriptAssetCreationResult Create(
			const std::filesystem::path& projectRoot,
			const std::string& sourcePath,
			const std::string& assetName);
		[[nodiscard]] static std::string MakeClassName(
			const std::string& assetName);
		[[nodiscard]] static std::string BuildTemplate(
			const std::string& className);
	};
}

#endif
