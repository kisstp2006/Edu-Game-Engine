#include "AssetBrowserModel.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <system_error>

namespace EGE
{
	namespace
	{
		std::string ToLower(std::string value)
		{
			std::transform(
				value.begin(), value.end(), value.begin(),
				[](unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
			return value;
		}

		bool ContainsInsensitive(
			const std::string& value,
			const std::string& lowercaseSearch)
		{
			return ToLower(value).find(lowercaseSearch) !=
				std::string::npos;
		}

		bool HasExtension(
			const std::string& extension,
			std::initializer_list<const char*> candidates)
		{
			return std::any_of(
				candidates.begin(), candidates.end(),
				[&extension](const char* candidate)
				{
					return extension == candidate;
				});
		}

		std::string MakeSourcePath(
			const std::filesystem::path& relativePath)
		{
			const std::string relative = relativePath.generic_string();
			return relative.empty() ? "Assets" : "Assets/" + relative;
		}
	}

	bool AssetBrowserModel::OpenProject(
		const std::filesystem::path& projectRoot,
		std::string& error)
	{
		std::error_code fileError;
		const std::filesystem::path normalizedRoot =
			std::filesystem::absolute(projectRoot, fileError)
				.lexically_normal();
		if (fileError ||
			!std::filesystem::is_directory(normalizedRoot, fileError))
		{
			error = "The active project directory is not available.";
			return false;
		}

		projectRoot_ = normalizedRoot;
		assetsRoot_ = projectRoot_ / "Assets";
		if (!std::filesystem::is_directory(assetsRoot_, fileError))
		{
			error = "The active project does not contain an Assets directory.";
			Reset();
			return false;
		}

		currentDirectory_.clear();
		return Refresh(error);
	}

	bool AssetBrowserModel::Refresh(std::string& error)
	{
		if (assetsRoot_.empty())
		{
			error = "No project is open.";
			return false;
		}

		AssetFolder newRoot;
		newRoot.name = "Assets";
		std::vector<AssetEntry> previousEntries;
		previousEntries.swap(entries_);

		if (!ScanFolder(assetsRoot_, {}, newRoot, error))
		{
			entries_.swap(previousEntries);
			return false;
		}

		rootFolder_ = std::move(newRoot);
		if (!ContainsFolder(rootFolder_, currentDirectory_))
			currentDirectory_.clear();
		return true;
	}

	void AssetBrowserModel::Reset()
	{
		projectRoot_.clear();
		assetsRoot_.clear();
		currentDirectory_.clear();
		rootFolder_ = {};
		entries_.clear();
	}

	bool AssetBrowserModel::NavigateTo(
		const std::filesystem::path& relativePath)
	{
		const std::filesystem::path normalized =
			relativePath.lexically_normal();
		if (normalized.is_absolute() ||
			normalized.generic_string().starts_with("..") ||
			!ContainsFolder(rootFolder_, normalized))
		{
			return false;
		}

		currentDirectory_ = normalized == "."
			? std::filesystem::path{}
			: normalized;
		return true;
	}

	bool AssetBrowserModel::NavigateUp()
	{
		if (currentDirectory_.empty())
			return false;
		return NavigateTo(currentDirectory_.parent_path());
	}

	std::vector<const AssetEntry*> AssetBrowserModel::Query(
		std::string_view search,
		AssetKind kindFilter) const
	{
		const std::string lowercaseSearch =
			ToLower(std::string(search));
		std::vector<const AssetEntry*> results;
		results.reserve(entries_.size());

		for (const AssetEntry& entry : entries_)
		{
			const bool searchActive = !lowercaseSearch.empty();
			if (!searchActive && entry.parentPath != currentDirectory_)
				continue;

			if (searchActive &&
				!ContainsInsensitive(entry.name, lowercaseSearch) &&
				!ContainsInsensitive(
					entry.relativePath.generic_string(), lowercaseSearch))
			{
				continue;
			}

			if (!entry.directory &&
				kindFilter != AssetKind::Unknown &&
				entry.kind != kindFilter)
			{
				continue;
			}

			results.push_back(&entry);
		}

		std::sort(
			results.begin(), results.end(),
			[](const AssetEntry* left, const AssetEntry* right)
			{
				if (left->directory != right->directory)
					return left->directory;
				return ToLower(left->name) < ToLower(right->name);
			});
		return results;
	}

	const std::filesystem::path& AssetBrowserModel::GetProjectRoot() const
	{
		return projectRoot_;
	}

	const std::filesystem::path& AssetBrowserModel::GetAssetsRoot() const
	{
		return assetsRoot_;
	}

	const std::filesystem::path&
	AssetBrowserModel::GetCurrentPath() const
	{
		return currentDirectory_;
	}

	const AssetFolder& AssetBrowserModel::GetFolderTree() const
	{
		return rootFolder_;
	}

	std::size_t AssetBrowserModel::GetAssetCount() const
	{
		return static_cast<std::size_t>(std::count_if(
			entries_.begin(), entries_.end(),
			[](const AssetEntry& entry)
			{
				return !entry.directory;
			}));
	}

	bool AssetBrowserModel::IsOpen() const
	{
		return !assetsRoot_.empty();
	}

	const AssetEntry* AssetBrowserModel::FindBySourcePath(
		std::string_view sourcePath) const
	{
		const auto iterator = std::find_if(
			entries_.begin(),
			entries_.end(),
			[sourcePath](const AssetEntry& entry)
			{
				return entry.sourcePath == sourcePath;
			});
		return iterator == entries_.end() ? nullptr : &*iterator;
	}

	AssetKind AssetBrowserModel::Classify(
		const std::filesystem::path& path)
	{
		const std::string filename =
			ToLower(path.filename().string());
		const std::string extension =
			ToLower(path.extension().string());

		if (filename.ends_with(".edumaterial.json"))
			return AssetKind::Material;
		if (filename.ends_with(".edustates.json"))
			return AssetKind::StateMachine;
		if (filename.ends_with(".edumesh.json"))
			return AssetKind::Mesh;
		if (HasExtension(extension, {".eduscene", ".scene"}))
			return AssetKind::Scene;
		if (HasExtension(
				extension,
				{".fbx", ".dae", ".gltf", ".glb", ".obj", ".3ds",
				 ".ply", ".stl", ".blend"}))
		{
			return AssetKind::Model;
		}
		if (HasExtension(
				extension,
				{".png", ".jpg", ".jpeg", ".tga", ".tif", ".tiff",
				 ".dds", ".hdr", ".bmp", ".ktx", ".ktx2"}))
		{
			return AssetKind::Texture;
		}
		if (HasExtension(
				extension,
				{".edumaterial", ".mat"}))
		{
			return AssetKind::Material;
		}
		if (HasExtension(
				extension,
				{".wav", ".ogg", ".mp3", ".flac"}))
		{
			return AssetKind::Audio;
		}
		if (HasExtension(
				extension,
				{".anim", ".animation", ".eduanim"}))
		{
			return AssetKind::Animation;
		}
		if (extension == ".as")
			return AssetKind::Script;
		if (HasExtension(
				extension,
				{".glsl", ".vert", ".frag", ".vs", ".fs",
				 ".shader", ".compute"}))
		{
			return AssetKind::Shader;
		}
		if (HasExtension(extension, {".ttf", ".otf"}))
			return AssetKind::Font;
		if (HasExtension(
				extension,
				{".json", ".txt", ".bin", ".cube", ".xml", ".yaml",
				 ".yml", ".csv"}))
		{
			return AssetKind::Data;
		}
		return AssetKind::Unknown;
	}

	const char* AssetBrowserModel::GetKindName(AssetKind kind)
	{
		switch (kind)
		{
		case AssetKind::Folder: return "Folder";
		case AssetKind::Scene: return "Scene";
		case AssetKind::Model: return "Model";
		case AssetKind::Mesh: return "Mesh";
		case AssetKind::Texture: return "Texture";
		case AssetKind::Material: return "Material";
		case AssetKind::Audio: return "Audio";
		case AssetKind::Animation: return "Animation";
		case AssetKind::StateMachine: return "State Machine";
		case AssetKind::Script: return "AngelScript";
		case AssetKind::Shader: return "Shader";
		case AssetKind::Font: return "Font";
		case AssetKind::Data: return "Data";
		case AssetKind::Unknown: return "Other";
		}
		return "Other";
	}

	bool AssetBrowserModel::ScanFolder(
		const std::filesystem::path& absolutePath,
		const std::filesystem::path& relativePath,
		AssetFolder& folder,
		std::string& error)
	{
		std::error_code iteratorError;
		std::filesystem::directory_iterator iterator(
			absolutePath,
			std::filesystem::directory_options::skip_permission_denied,
			iteratorError);
		if (iteratorError)
		{
			error = "Cannot read Assets/" +
				relativePath.generic_string() + ": " +
				iteratorError.message();
			return false;
		}

		for (const std::filesystem::directory_entry& item : iterator)
		{
			std::error_code itemError;
			const std::filesystem::path childRelative =
				relativePath / item.path().filename();

			if (item.is_directory(itemError))
			{
				AssetEntry entry;
				entry.relativePath = childRelative;
				entry.parentPath = relativePath;
				entry.sourcePath = MakeSourcePath(childRelative);
				entry.name = item.path().filename().string();
				entry.kind = AssetKind::Folder;
				entry.directory = true;
				entries_.push_back(std::move(entry));

				AssetFolder childFolder;
				childFolder.relativePath = childRelative;
				childFolder.name = item.path().filename().string();
				if (!ScanFolder(
						item.path(), childRelative, childFolder, error))
				{
					return false;
				}
				folder.children.push_back(std::move(childFolder));
				continue;
			}

			if (!item.is_regular_file(itemError))
				continue;

			AssetEntry entry;
			entry.relativePath = childRelative;
			entry.parentPath = relativePath;
			entry.sourcePath = MakeSourcePath(childRelative);
			entry.name = item.path().filename().string();
			entry.extension =
				ToLower(item.path().extension().string());
			entry.kind = Classify(item.path());
			entry.size = item.file_size(itemError);
			entries_.push_back(std::move(entry));
		}

		std::sort(
			folder.children.begin(), folder.children.end(),
			[](const AssetFolder& left, const AssetFolder& right)
			{
				return ToLower(left.name) < ToLower(right.name);
			});
		return true;
	}

	bool AssetBrowserModel::ContainsFolder(
		const AssetFolder& folder,
		const std::filesystem::path& relativePath) const
	{
		if (folder.relativePath == relativePath ||
			(relativePath == "." && folder.relativePath.empty()))
		{
			return true;
		}

		return std::any_of(
			folder.children.begin(), folder.children.end(),
			[this, &relativePath](const AssetFolder& child)
			{
				return ContainsFolder(child, relativePath);
			});
	}
}
