#pragma once

#include "AssetBrowserModel.h"
#include "Panel.h"
#include "Resource.h"

#include "ImportAnimationDlg.h"
#include "ImportModelDlg.h"
#include "ImportTextureDlg.h"
#include "ShowTextureDlg.h"
#include <imgui.h>
#include "imgui-filebrowser/imfilebrowser.h"

#include <filesystem>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

class PanelAssets final : public Panel
{
public:
	PanelAssets();

	void Draw() override;
	void ResetProjectState();

private:
	enum class ViewMode
	{
		Grid,
		List
	};

	enum class CreateAssetKind
	{
		None,
		Folder,
		MaterialMetallicRoughness,
		MaterialSpecularGlossiness,
		StateMachine,
		MeshPlane,
		MeshCube,
		MeshSphere,
		MeshCylinder,
		MeshCone,
		MeshTorus
	};

	struct MeshCreationForm
	{
		float width = 1.0f;
		float height = 1.0f;
		float radius = 0.5f;
		float innerRadius = 0.35f;
		float outerRadius = 1.0f;
		int slices = 24;
		int stacks = 12;
	};

	struct ImportInfo
	{
		UID primaryUid = 0;
		Resource::Type primaryType = Resource::unknown;
		std::size_t resourceCount = 0;
		std::vector<UID> resourceUids;
	};

	void DrawDialogs();
	void DrawToolbar();
	void DrawFolderPane(float height);
	void DrawContentPane(float height);
	void DrawStatusBar(
		const std::vector<const EGE::AssetEntry*>& entries) const;
	void DrawFolderNode(const EGE::AssetFolder& folder);
	void DrawGrid(
		const std::vector<const EGE::AssetEntry*>& entries);
	void DrawList(
		const std::vector<const EGE::AssetEntry*>& entries);
	void DrawGridItem(
		const EGE::AssetEntry& entry,
		const std::vector<const EGE::AssetEntry*>& entries,
		std::size_t index,
		float tileWidth);
	void DrawListItem(
		const EGE::AssetEntry& entry,
		const std::vector<const EGE::AssetEntry*>& entries,
		std::size_t index);
	void DrawAssetIcon(
		ImDrawList* drawList,
		const EGE::AssetEntry& entry,
		const ImVec2& minimum,
		const ImVec2& maximum) const;
	void DrawAssetContextMenu(const EGE::AssetEntry& entry);
	void DrawCreateMenu();
	void DrawCreateDialog();
	void DrawImportMenu();
	void DrawImportOptions();

	void EnsureProject();
	void Refresh();
	void RebuildImportIndex();
	void NavigateTo(
		const std::filesystem::path& directory,
		bool addToHistory = true);
	void NavigateBackward();
	void NavigateForward();
	void CommitPendingNavigation();
	void HandleSelection(
		const std::vector<const EGE::AssetEntry*>& entries,
		std::size_t index,
		bool fromContextMenu);
	void SyncInspectorSelection(
		const std::vector<const EGE::AssetEntry*>& entries,
		std::size_t preferredIndex);
	void SelectInInspector(const EGE::AssetEntry& entry);
	void OpenAssetEditor(const EGE::AssetEntry& entry);
	void HandleAssetInteractions(
		const EGE::AssetEntry& entry,
		const std::vector<const EGE::AssetEntry*>& entries,
		std::size_t index);

	void OpenImportDialog(Resource::Type type);
	void BeginCreate(CreateAssetKind kind);
	void CreatePendingAsset();
	void CreateFolder();
	std::string BuildCreateSourcePath() const;
	std::string MakeUniqueCreateName(
		const char* baseName,
		const char* extension) const;
	static bool IsValidAssetName(const char* name);
	static const char* CreateExtension(CreateAssetKind kind);
	static const char* CreateTypeName(CreateAssetKind kind);
	void ImportResource(
		const std::string& sourcePath,
		Resource::Type type);
	Resource::Type ResourceTypeFor(
		const EGE::AssetEntry& entry) const;
	const ImportInfo* FindImportInfo(
		const EGE::AssetEntry& entry) const;
	UID ResolvePrimaryResource(
		const EGE::AssetEntry& entry,
		const ImportInfo& info) const;

	static std::string NormalizeAssetKey(std::string path);
	static std::string ShortenText(
		const std::string& text,
		float availableWidth);
	static std::string FormatFileSize(std::uintmax_t bytes);
	static ImVec4 KindColor(EGE::AssetKind kind);
	static const char* KindGlyph(EGE::AssetKind kind);

private:
	EGE::AssetBrowserModel browser_;
	std::unordered_map<std::string, ImportInfo> importIndex_;

	std::vector<std::filesystem::path> history_;
	std::size_t historyIndex_ = 0;
	std::optional<std::filesystem::path> pendingNavigation_;

	std::set<std::string> selection_;
	std::size_t selectionAnchor_ =
		std::numeric_limits<std::size_t>::max();

	char search_[256] = {};
	EGE::AssetKind kindFilter_ = EGE::AssetKind::Unknown;
	ViewMode viewMode_ = ViewMode::Grid;
	float thumbnailSize_ = 72.0f;
	float folderPaneWidth_ = 210.0f;

	std::string errorMessage_;
	std::string statusMessage_;
	CreateAssetKind createKind_ = CreateAssetKind::None;
	bool openCreateDialog_ = false;
	char createName_[128] = {};
	MeshCreationForm meshCreation_;

	ImGui::FileBrowser fileDialog_;
	Resource::Type waitingToImport_ = Resource::unknown;
	ImportTexturesDlg textureDialog_;
	ImportAnimationDlg animationDialog_;
	ImportModelDlg modelDialog_;
	ShowTextureDlg texturePreview_;
};
