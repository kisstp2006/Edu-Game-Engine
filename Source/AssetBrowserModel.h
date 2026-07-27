#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace EGE
{
	enum class AssetKind : std::uint8_t
	{
		Folder,
		Scene,
		Model,
		Mesh,
		Texture,
		Material,
		Audio,
		Animation,
		StateMachine,
		Script,
		Shader,
		Font,
		Data,
		Unknown
	};

	struct AssetEntry
	{
		std::filesystem::path relativePath;
		std::filesystem::path parentPath;
		std::string sourcePath;
		std::string name;
		std::string extension;
		AssetKind kind = AssetKind::Unknown;
		std::uintmax_t size = 0;
		bool directory = false;
	};

	struct AssetFolder
	{
		std::filesystem::path relativePath;
		std::string name;
		std::vector<AssetFolder> children;
	};

	class AssetBrowserModel
	{
	public:
		bool OpenProject(
			const std::filesystem::path& projectRoot,
			std::string& error);
		bool Refresh(std::string& error);
		void Reset();

		bool NavigateTo(const std::filesystem::path& relativePath);
		bool NavigateUp();

		std::vector<const AssetEntry*> Query(
			std::string_view search,
			AssetKind kindFilter) const;

		const std::filesystem::path& GetProjectRoot() const;
		const std::filesystem::path& GetAssetsRoot() const;
		const std::filesystem::path& GetCurrentPath() const;
		const AssetFolder& GetFolderTree() const;
		std::size_t GetAssetCount() const;
		bool IsOpen() const;
		const AssetEntry* FindBySourcePath(
			std::string_view sourcePath) const;

		static AssetKind Classify(const std::filesystem::path& path);
		static const char* GetKindName(AssetKind kind);

	private:
		bool ScanFolder(
			const std::filesystem::path& absolutePath,
			const std::filesystem::path& relativePath,
			AssetFolder& folder,
			std::string& error);
		bool ContainsFolder(
			const AssetFolder& folder,
			const std::filesystem::path& relativePath) const;

	private:
		std::filesystem::path projectRoot_;
		std::filesystem::path assetsRoot_;
		std::filesystem::path currentDirectory_;
		AssetFolder rootFolder_;
		std::vector<AssetEntry> entries_;
	};
}
