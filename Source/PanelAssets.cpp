#include "PanelAssets.h"

#include "Application.h"
#include "EditorDialog.h"
#include "EditorAssetSelection.h"
#include "ModuleEditor.h"
#include "ModuleFileSystem.h"
#include "ModuleLevelManager.h"
#include "ModuleResources.h"
#include "ResourceTexture.h"
#include "Project/VsCodeWorkspace.h"
#include "Scripting/ScriptAsset.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <SDL_scancode.h>

namespace
{
	constexpr float StatusBarHeight = 24.0f;
	constexpr float ContentGap = 6.0f;

	ImU32 WithAlpha(const ImVec4& color, float alpha)
	{
		return ImGui::ColorConvertFloat4ToU32(
			ImVec4(color.x, color.y, color.z, alpha));
	}

	bool IsPathInside(
		const std::filesystem::path& child,
		const std::filesystem::path& parent)
	{
		const std::filesystem::path relative =
			child.lexically_relative(parent);
		if (relative.empty())
			return child == parent;
		const std::string text = relative.generic_string();
		return text != ".." && !text.starts_with("../");
	}

	float ButtonWidth(const char* label)
	{
		return ImGui::CalcTextSize(label).x +
			ImGui::GetStyle().FramePadding.x * 2.0f;
	}

	bool ContinueControlLine(float nextWidth)
	{
		const float contentRight =
			ImGui::GetWindowPos().x +
			ImGui::GetWindowContentRegionMax().x;
		if (ImGui::GetItemRectMax().x +
				ImGui::GetStyle().ItemSpacing.x +
				nextWidth >
			contentRight)
		{
			return false;
		}

		ImGui::SameLine();
		return true;
	}
}

PanelAssets::PanelAssets()
	: Panel("Assets")
{
	width = 900;
	height = 320;
	posx = 350;
	posy = 700;
	fileDialog_.SetTitle("Import asset");
}

void PanelAssets::Draw()
{
	if (!App->GetActiveProject())
	{
		ImGui::TextDisabled(
			"Choose a project to browse its assets.");
		return;
	}

	DrawDialogs();
	EnsureProject();
	DrawToolbar();
	ImGui::Separator();

	const float contentHeight = std::max(
		80.0f,
		ImGui::GetContentRegionAvail().y - StatusBarHeight);
	DrawFolderPane(contentHeight);
	ImGui::SameLine(0.0f, 0.0f);
	ImGui::InvisibleButton(
		"##AssetFolderSplitter",
		ImVec2(4.0f, contentHeight));
	if (ImGui::IsItemHovered() || ImGui::IsItemActive())
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	if (ImGui::IsItemActive())
	{
		folderPaneWidth_ = std::clamp(
			folderPaneWidth_ + ImGui::GetIO().MouseDelta.x,
			150.0f,
			420.0f);
	}
	ImGui::SameLine(0.0f, ContentGap);
	DrawContentPane(contentHeight);
	CommitPendingNavigation();
}

void PanelAssets::ResetProjectState()
{
	fileDialog_.Close();
	fileDialog_.ClearSelected();
	textureDialog_.ClearSelection();
	audioDialog_.ClearSelection();
	animationDialog_.ClearSelection();
	modelDialog_.ClearSelection();
	texturePreview_.Clear();

	browser_.Reset();
	importIndex_.clear();
	history_.clear();
	historyIndex_ = 0;
	pendingNavigation_.reset();
	selection_.clear();
	selectionAnchor_ = std::numeric_limits<std::size_t>::max();
	search_[0] = '\0';
	waitingToImport_ = Resource::unknown;
	errorMessage_.clear();
	statusMessage_.clear();
	createKind_ = CreateAssetKind::None;
	openCreateDialog_ = false;
	createName_[0] = '\0';
	meshCreation_ = {};
}

void PanelAssets::RefreshProjectAssets()
{
	EnsureProject();
	Refresh();
}

void PanelAssets::DrawDialogs()
{
	DrawCreateDialog();
	fileDialog_.Display();
	if (fileDialog_.HasSelected())
	{
		std::string sourcePath;
		bool copiedIntoProject = false;
		if (PrepareImportSource(
				fileDialog_.GetSelected(),
				sourcePath,
				copiedIntoProject))
		{
			if (copiedIntoProject)
			{
				Refresh();
				statusMessage_ =
					std::filesystem::path(sourcePath).filename().string() +
					" copied into the current Assets folder.";
			}
			ImportResource(sourcePath, waitingToImport_);
		}

		fileDialog_.ClearSelected();
		waitingToImport_ = Resource::unknown;
	}

	textureDialog_.Display();
	if (textureDialog_.HasSelection())
	{
		const UID uid = App->resources->ImportTexture(
			textureDialog_.GetFile().c_str(),
			textureDialog_.GetOptions());
		statusMessage_ = uid != 0
			? "Texture imported."
			: "Texture import failed.";
		textureDialog_.ClearSelection();
		RebuildImportIndex();
	}

	audioDialog_.Display();
	if (audioDialog_.HasSelection())
	{
		const UID uid = App->resources->ImportAudio(
			audioDialog_.GetFile().c_str(),
			audioDialog_.GetOptions());
		statusMessage_ = uid != 0
			? "Audio imported."
			: "Audio import failed.";
		audioDialog_.ClearSelection();
		RebuildImportIndex();
	}

	animationDialog_.Display();
	if (animationDialog_.HasSelection())
	{
		const EGE::AnimationImportOptions options =
			animationDialog_.GetOptions();
		std::size_t importedClips = 0;
		for (const EGE::AnimationClipImportRange& clip : options.clips)
		{
			if (App->resources->ImportAnimation(
					animationDialog_.GetFile().c_str(),
					clip.firstFrame,
					clip.lastFrame,
					clip.name.c_str(),
					options) != 0)
			{
				++importedClips;
			}
		}
		statusMessage_ = importedClips > 0
			? std::to_string(importedClips) +
				" animation clip(s) imported."
			: "Animation import failed.";
		animationDialog_.ClearSelection();
		RebuildImportIndex();
	}

	modelDialog_.Display();
	if (modelDialog_.HasSelection())
	{
		const UID uid = App->resources->ImportModel(
			modelDialog_.GetFile().c_str(),
			modelDialog_.GetOptions());
		statusMessage_ = uid != 0
			? "Model imported."
			: "Model import failed.";
		modelDialog_.ClearSelection();
		RebuildImportIndex();
	}

	texturePreview_.Display();
}

void PanelAssets::DrawToolbar()
{
	const ImGuiIO& io = ImGui::GetIO();
	const bool toolbarFocused =
		ImGui::IsWindowFocused(
			ImGuiFocusedFlags_RootAndChildWindows);
	const bool acceptsShortcut =
		toolbarFocused &&
		!io.WantTextInput &&
		!ImGui::IsAnyItemActive();
	if (acceptsShortcut &&
		ImGui::IsKeyPressed(SDL_SCANCODE_F5, false))
	{
		Refresh();
	}
	if (acceptsShortcut && io.KeyAlt &&
		ImGui::IsKeyPressed(SDL_SCANCODE_LEFT, false))
	{
		NavigateBackward();
	}
	if (acceptsShortcut && io.KeyAlt &&
		ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT, false))
	{
		NavigateForward();
	}
	if (acceptsShortcut && !io.KeyAlt && !io.KeyCtrl &&
		ImGui::IsKeyPressed(SDL_SCANCODE_BACKSPACE, false) &&
		!browser_.GetCurrentPath().empty())
	{
		NavigateTo(browser_.GetCurrentPath().parent_path());
	}
	const bool focusSearch =
		acceptsShortcut &&
		io.KeyCtrl &&
		ImGui::IsKeyPressed(SDL_SCANCODE_F, false);

	const bool hasPrevious = historyIndex_ > 0;
	const bool hasNext =
		!history_.empty() && historyIndex_ + 1 < history_.size();
	const bool hasParent =
		!browser_.GetCurrentPath().empty();

	if (!hasPrevious)
		ImGui::PushStyleColor(
			ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	if (ImGui::Button("<", ImVec2(28.0f, 0.0f)) && hasPrevious)
		NavigateBackward();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Back (Alt+Left)");
	if (!hasPrevious)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	if (!hasNext)
		ImGui::PushStyleColor(
			ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	if (ImGui::Button(">", ImVec2(28.0f, 0.0f)) && hasNext)
		NavigateForward();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Forward (Alt+Right)");
	if (!hasNext)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	if (!hasParent)
		ImGui::PushStyleColor(
			ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	if (ImGui::Button("^", ImVec2(28.0f, 0.0f)) && hasParent)
		NavigateTo(browser_.GetCurrentPath().parent_path());
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Parent folder (Backspace)");
	if (!hasParent)
		ImGui::PopStyleColor();

	ImGui::SameLine();
	if (ImGui::Button("Assets"))
		NavigateTo({});

	std::filesystem::path breadcrumb;
	for (const std::filesystem::path& component :
		browser_.GetCurrentPath())
	{
		breadcrumb /= component;
		const std::string label = component.string();
		const float separatorWidth = ImGui::CalcTextSize("/").x;
		const float segmentWidth =
			separatorWidth +
			ImGui::GetStyle().ItemSpacing.x +
			ButtonWidth(label.c_str());
		if (ContinueControlLine(segmentWidth))
		{
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("/");
			ImGui::SameLine();
		}
		if (ImGui::Button(label.c_str()))
			NavigateTo(breadcrumb);
	}

	ImGui::Spacing();
	if (ImGui::Button("Create"))
		ImGui::OpenPopup("AssetCreateMenu");
	if (ImGui::BeginPopup("AssetCreateMenu"))
	{
		DrawCreateMenu();
		ImGui::EndPopup();
	}

	ContinueControlLine(ButtonWidth("Import"));
	if (ImGui::Button("Import"))
		ImGui::OpenPopup("AssetImportMenu");
	DrawImportMenu();

	ContinueControlLine(ButtonWidth("Refresh"));
	if (ImGui::Button("Refresh"))
		Refresh();
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Refresh assets (F5)");

	const float clearSearchWidth = ButtonWidth("X");
	const float availableSearchWidth =
		ImGui::GetContentRegionAvail().x;
	const float searchWidth = std::min(
		250.0f,
		std::max(
			1.0f,
			availableSearchWidth -
				clearSearchWidth -
				ImGui::GetStyle().ItemSpacing.x));
	if (focusSearch)
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(searchWidth);
	ImGui::InputTextWithHint(
		"##AssetSearch",
		"Search all assets... (Ctrl+F)",
		search_,
		sizeof(search_));
	ImGui::SameLine();
	if (ImGui::Button("X##ClearAssetSearch") && search_[0] != '\0')
		search_[0] = '\0';
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Clear search");

	const char* filterPreview = kindFilter_ == EGE::AssetKind::Unknown
		? "All types"
		: EGE::AssetBrowserModel::GetKindName(kindFilter_);
	const float filterWidth = 115.0f;
	ContinueControlLine(filterWidth);
	ImGui::SetNextItemWidth(filterWidth);
	if (ImGui::BeginCombo("##AssetKindFilter", filterPreview))
	{
		constexpr std::array<EGE::AssetKind, 13> filters = {
			EGE::AssetKind::Unknown,
			EGE::AssetKind::Scene,
			EGE::AssetKind::Model,
			EGE::AssetKind::Mesh,
			EGE::AssetKind::Texture,
			EGE::AssetKind::Material,
			EGE::AssetKind::Audio,
			EGE::AssetKind::Animation,
			EGE::AssetKind::StateMachine,
			EGE::AssetKind::Script,
			EGE::AssetKind::Shader,
			EGE::AssetKind::Font,
			EGE::AssetKind::Data
		};
		for (EGE::AssetKind kind : filters)
		{
			const char* label = kind == EGE::AssetKind::Unknown
				? "All types"
				: EGE::AssetBrowserModel::GetKindName(kind);
			const bool selected = kindFilter_ == kind;
			if (ImGui::Selectable(label, selected))
				kindFilter_ = kind;
			if (selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	const char* viewModeLabel =
		viewMode_ == ViewMode::Grid ? "Grid" : "List";
	ContinueControlLine(ButtonWidth(viewModeLabel));
	if (ImGui::Button(
			viewModeLabel))
	{
		viewMode_ = viewMode_ == ViewMode::Grid
			? ViewMode::List
			: ViewMode::Grid;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(
			viewMode_ == ViewMode::Grid
				? "Switch to list view"
				: "Switch to grid view");
	}

	if (viewMode_ == ViewMode::Grid)
	{
		const float thumbnailControlWidth = 110.0f;
		ContinueControlLine(thumbnailControlWidth);
		ImGui::SetNextItemWidth(thumbnailControlWidth);
		ImGui::SliderFloat(
			"##AssetThumbnailSize",
			&thumbnailSize_,
			48.0f,
			112.0f,
			"Size %.0f");
	}
}

void PanelAssets::DrawFolderPane(float height)
{
	const float maximumWidth =
		std::max(120.0f, ImGui::GetContentRegionAvail().x * 0.35f);
	folderPaneWidth_ = std::clamp(
		folderPaneWidth_, 150.0f, maximumWidth);

	ImGui::PushStyleColor(
		ImGuiCol_ChildBg,
		ImVec4(0.045f, 0.054f, 0.072f, 0.72f));
	if (ImGui::BeginChild(
			"##AssetFolders",
			ImVec2(folderPaneWidth_, height),
			true))
	{
		ImGui::TextDisabled("PROJECT");
		ImGui::Spacing();
		if (browser_.IsOpen())
			DrawFolderNode(browser_.GetFolderTree());
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void PanelAssets::DrawContentPane(float height)
{
	ImGui::BeginGroup();
	if (ImGui::BeginChild(
			"##AssetContent",
			ImVec2(0.0f, height),
			true))
	{
		const std::vector<const EGE::AssetEntry*> entries =
			browser_.Query(search_, kindFilter_);

		if (!errorMessage_.empty())
		{
			ImGui::PushStyleColor(
				ImGuiCol_Text, ImVec4(0.96f, 0.43f, 0.43f, 1.0f));
			ImGui::TextWrapped("%s", errorMessage_.c_str());
			ImGui::PopStyleColor();
			ImGui::Separator();
		}

		if (entries.empty())
		{
			const ImVec2 available = ImGui::GetContentRegionAvail();
			ImGui::SetCursorPosY(
				ImGui::GetCursorPosY() +
				std::max(12.0f, available.y * 0.35f));
			const char* emptyText = search_[0] != '\0'
				? "No assets match this search."
				: "This folder is empty.";
			const float textWidth =
				ImGui::CalcTextSize(emptyText).x;
			ImGui::SetCursorPosX(
				ImGui::GetCursorPosX() +
				std::max(0.0f, (available.x - textWidth) * 0.5f));
			ImGui::TextDisabled("%s", emptyText);
		}
		else if (viewMode_ == ViewMode::Grid)
		{
			DrawGrid(entries);
		}
		else
		{
			DrawList(entries);
		}

		if (ImGui::BeginPopupContextWindow(
				"AssetBackgroundMenu",
				ImGuiPopupFlags_MouseButtonRight |
					ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::BeginMenu("Create"))
			{
				DrawCreateMenu();
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Import asset"))
			{
				DrawImportOptions();
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Refresh"))
				Refresh();
			ImGui::EndPopup();
		}

		DrawStatusBar(entries);
	}
	ImGui::EndChild();
	ImGui::EndGroup();
}

void PanelAssets::DrawStatusBar(
	const std::vector<const EGE::AssetEntry*>& entries) const
{
	const float y = ImGui::GetWindowHeight() - StatusBarHeight;
	ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), y));
	ImGui::Separator();

	const std::size_t fileCount = static_cast<std::size_t>(
		std::count_if(
			entries.begin(), entries.end(),
			[](const EGE::AssetEntry* entry)
			{
				return !entry->directory;
			}));
	const std::size_t folderCount = entries.size() - fileCount;
	ImGui::TextDisabled(
		"%zu folder(s)  |  %zu asset(s)  |  %zu selected",
		folderCount,
		fileCount,
		selection_.size());

	if (!statusMessage_.empty())
	{
		ImGui::SameLine();
		ImGui::TextColored(
			ImVec4(0.36f, 0.78f, 0.64f, 1.0f),
			"|  %s",
			statusMessage_.c_str());
	}
}

void PanelAssets::DrawFolderNode(const EGE::AssetFolder& folder)
{
	const bool selected =
		folder.relativePath == browser_.GetCurrentPath();
	const bool leaf = folder.children.empty();
	ImGuiTreeNodeFlags flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;
	if (selected)
		flags |= ImGuiTreeNodeFlags_Selected;
	if (leaf)
		flags |= ImGuiTreeNodeFlags_Leaf |
			ImGuiTreeNodeFlags_NoTreePushOnOpen;
	if (folder.relativePath.empty())
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);

	ImGui::PushID(folder.relativePath.generic_string().c_str());
	const bool open = ImGui::TreeNodeEx(
		"##Folder",
		flags,
		"%s",
		folder.name.c_str());
	if (ImGui::IsItemClicked())
		NavigateTo(folder.relativePath);

	if (open && !leaf)
	{
		for (const EGE::AssetFolder& child : folder.children)
			DrawFolderNode(child);
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void PanelAssets::DrawGrid(
	const std::vector<const EGE::AssetEntry*>& entries)
{
	const float tileWidth = thumbnailSize_ + 36.0f;
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const int columnCount = std::max(
		1, static_cast<int>(availableWidth / tileWidth));

	for (std::size_t index = 0; index < entries.size(); ++index)
	{
		if (index % static_cast<std::size_t>(columnCount) != 0)
			ImGui::SameLine();
		DrawGridItem(*entries[index], entries, index, tileWidth - 8.0f);
	}
}

void PanelAssets::DrawList(
	const std::vector<const EGE::AssetEntry*>& entries)
{
	const float availableWidth =
		std::max(460.0f, ImGui::GetContentRegionAvail().x);
	const float typeWidth = 115.0f;
	const float statusWidth = 115.0f;
	const float sizeWidth = 90.0f;
	const float nameWidth = std::max(
		160.0f,
		availableWidth - typeWidth - statusWidth - sizeWidth);
	ImGui::Columns(4, "AssetListColumns", false);
	ImGui::SetColumnWidth(0, nameWidth);
	ImGui::SetColumnWidth(1, typeWidth);
	ImGui::SetColumnWidth(2, statusWidth);
	ImGui::SetColumnWidth(3, sizeWidth);
	ImGui::TextDisabled("Name");
	ImGui::NextColumn();
	ImGui::TextDisabled("Type");
	ImGui::NextColumn();
	ImGui::TextDisabled("Status");
	ImGui::NextColumn();
	ImGui::TextDisabled("Size");
	ImGui::NextColumn();
	ImGui::Separator();

	for (std::size_t index = 0; index < entries.size(); ++index)
		DrawListItem(*entries[index], entries, index);

	ImGui::Columns(1);
}

void PanelAssets::DrawGridItem(
	const EGE::AssetEntry& entry,
	const std::vector<const EGE::AssetEntry*>& entries,
	std::size_t index,
	float tileWidth)
{
	const float tileHeight = thumbnailSize_ + 50.0f;
	const bool selected =
		selection_.contains(entry.sourcePath);
	ImGui::PushID(entry.sourcePath.c_str());
	const ImVec2 start = ImGui::GetCursorScreenPos();
	ImGui::InvisibleButton(
		"##AssetTile",
		ImVec2(tileWidth, tileHeight));
	const bool hovered = ImGui::IsItemHovered();

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec4 accent = KindColor(entry.kind);
	const ImU32 background = selected
		? WithAlpha(accent, 0.25f)
		: hovered
			? ImGui::GetColorU32(ImGuiCol_FrameBgHovered)
			: ImGui::GetColorU32(ImGuiCol_FrameBg);
	drawList->AddRectFilled(
		start,
		start + ImVec2(tileWidth, tileHeight),
		background,
		6.0f);
	drawList->AddRect(
		start,
		start + ImVec2(tileWidth, tileHeight),
		selected
			? WithAlpha(accent, 0.95f)
			: ImGui::GetColorU32(ImGuiCol_Border),
		6.0f,
		ImDrawCornerFlags_All,
		selected ? 1.5f : 1.0f);

	const ImVec2 iconMinimum =
		start + ImVec2(12.0f, 10.0f);
	const ImVec2 iconMaximum =
		iconMinimum + ImVec2(
			tileWidth - 24.0f,
			thumbnailSize_ - 4.0f);
	DrawAssetIcon(drawList, entry, iconMinimum, iconMaximum);

	const std::string displayName =
		ShortenText(entry.name, tileWidth - 14.0f);
	const ImVec2 textSize =
		ImGui::CalcTextSize(displayName.c_str());
	drawList->AddText(
		ImVec2(
			start.x + (tileWidth - textSize.x) * 0.5f,
			start.y + thumbnailSize_ + 13.0f),
		ImGui::GetColorU32(ImGuiCol_Text),
		displayName.c_str());

	const char* kindName =
		EGE::AssetBrowserModel::GetKindName(entry.kind);
	const ImVec2 kindSize = ImGui::CalcTextSize(kindName);
	drawList->AddText(
		ImVec2(
			start.x + (tileWidth - kindSize.x) * 0.5f,
			start.y + thumbnailSize_ + 31.0f),
		WithAlpha(accent, 0.88f),
		kindName);

	if (!entry.directory && FindImportInfo(entry))
	{
		drawList->AddCircleFilled(
			start + ImVec2(tileWidth - 11.0f, 11.0f),
			4.0f,
			IM_COL32(74, 205, 145, 255));
	}

	HandleAssetInteractions(entry, entries, index);
	ImGui::PopID();
}

void PanelAssets::DrawListItem(
	const EGE::AssetEntry& entry,
	const std::vector<const EGE::AssetEntry*>& entries,
	std::size_t index)
{
	const bool selected =
		selection_.contains(entry.sourcePath);
	ImGui::PushID(entry.sourcePath.c_str());
	const ImVec2 rowStart = ImGui::GetCursorScreenPos();
	ImGui::Selectable(
		"##AssetRow",
		selected,
		ImGuiSelectableFlags_SpanAllColumns,
		ImVec2(0.0f, 24.0f));
	const ImVec4 accent = KindColor(entry.kind);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->AddRectFilled(
		rowStart + ImVec2(4.0f, 4.0f),
		rowStart + ImVec2(20.0f, 20.0f),
		WithAlpha(accent, 0.88f),
		entry.directory ? 3.0f : 4.0f);
	drawList->AddText(
		rowStart + ImVec2(27.0f, 4.0f),
		ImGui::GetColorU32(ImGuiCol_Text),
		entry.name.c_str());
	HandleAssetInteractions(entry, entries, index);

	ImGui::NextColumn();
	ImGui::TextColored(
		accent,
		"%s",
		EGE::AssetBrowserModel::GetKindName(entry.kind));
	ImGui::NextColumn();
	if (entry.directory)
	{
		ImGui::TextDisabled("-");
	}
	else if (const ImportInfo* info = FindImportInfo(entry))
	{
		ImGui::TextColored(
			ImVec4(0.36f, 0.78f, 0.64f, 1.0f),
			info->resourceCount > 1 ? "%zu resources" : "Imported",
			info->resourceCount);
	}
	else if (ResourceTypeFor(entry) != Resource::unknown)
	{
		ImGui::TextDisabled("Source");
	}
	else
	{
		ImGui::TextDisabled("-");
	}
	ImGui::NextColumn();
	ImGui::TextDisabled(
		"%s",
		entry.directory ? "-" : FormatFileSize(entry.size).c_str());
	ImGui::NextColumn();
	ImGui::PopID();
}

void PanelAssets::DrawAssetIcon(
	ImDrawList* drawList,
	const EGE::AssetEntry& entry,
	const ImVec2& minimum,
	const ImVec2& maximum) const
{
	const ImVec4 accent = KindColor(entry.kind);
	if (entry.directory)
	{
		const float tabWidth = (maximum.x - minimum.x) * 0.42f;
		const float tabHeight = 8.0f;
		drawList->AddRectFilled(
			minimum + ImVec2(8.0f, 8.0f),
			ImVec2(minimum.x + 8.0f + tabWidth, minimum.y + 18.0f),
			WithAlpha(accent, 0.78f),
			4.0f,
			ImDrawCornerFlags_Top);
		drawList->AddRectFilled(
			minimum + ImVec2(8.0f, 8.0f + tabHeight),
			maximum - ImVec2(8.0f, 7.0f),
			WithAlpha(accent, 0.90f),
			5.0f);
		drawList->AddRectFilled(
			minimum + ImVec2(12.0f, 25.0f),
			maximum - ImVec2(12.0f, 11.0f),
			WithAlpha(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.08f),
			4.0f);
		return;
	}

	drawList->AddRectFilled(
		minimum + ImVec2(8.0f, 4.0f),
		maximum - ImVec2(8.0f, 4.0f),
		WithAlpha(accent, 0.18f),
		7.0f);
	drawList->AddRect(
		minimum + ImVec2(8.0f, 4.0f),
		maximum - ImVec2(8.0f, 4.0f),
		WithAlpha(accent, 0.70f),
		7.0f);

	const char* glyph = KindGlyph(entry.kind);
	const ImVec2 glyphSize = ImGui::CalcTextSize(glyph);
	const ImVec2 center = (minimum + maximum) * 0.5f;
	drawList->AddText(
		center - glyphSize * 0.5f,
		WithAlpha(accent, 1.0f),
		glyph);
}

void PanelAssets::DrawAssetContextMenu(
	const EGE::AssetEntry& entry)
{
	if (entry.directory)
	{
		if (ImGui::MenuItem("Open"))
			pendingNavigation_ = entry.relativePath;
	}
	else
	{
		if (entry.kind == EGE::AssetKind::Scene)
		{
			if (ImGui::MenuItem("Open Scene"))
				OpenScene(entry);
			ImGui::Separator();
		}

		const Resource::Type resourceType = ResourceTypeFor(entry);
		const ImportInfo* importInfo = FindImportInfo(entry);
		if (resourceType != Resource::unknown && !importInfo)
		{
			if (ImGui::MenuItem("Import"))
				ImportResource(entry.sourcePath, resourceType);
		}
		else if (importInfo)
		{
			ImGui::MenuItem("Imported", nullptr, false, false);
		}

		if ((entry.kind == EGE::AssetKind::Material ||
			 entry.kind == EGE::AssetKind::StateMachine ||
			 entry.kind == EGE::AssetKind::Mesh) &&
			importInfo &&
			(importInfo->primaryType == Resource::material ||
			 importInfo->primaryType == Resource::state_machine ||
			 importInfo->primaryType == Resource::mesh))
		{
			if (ImGui::MenuItem("Open Editor"))
				OpenAssetEditor(entry);
		}

		if (entry.kind == EGE::AssetKind::Script &&
			ImGui::MenuItem("Open in VS Code"))
		{
			OpenScriptInVsCode(entry);
		}

		if (importInfo &&
			importInfo->primaryType == Resource::texture &&
			ImGui::MenuItem("Preview"))
		{
			ResourceTexture* texture =
				App->resources->GetTexture(importInfo->primaryUid);
			if (texture)
				texturePreview_.Open(nullptr, texture);
		}
	}

	ImGui::Separator();
	if (ImGui::MenuItem("Copy asset path"))
		ImGui::SetClipboardText(entry.sourcePath.c_str());
}

void PanelAssets::DrawCreateMenu()
{
	if (ImGui::MenuItem("Folder"))
		BeginCreate(CreateAssetKind::Folder);
	if (ImGui::MenuItem("Scene"))
		BeginCreate(CreateAssetKind::Scene);
	if (ImGui::MenuItem("AngelScript"))
		BeginCreate(CreateAssetKind::AngelScript);

	ImGui::Separator();
	if (ImGui::BeginMenu("Material"))
	{
		if (ImGui::MenuItem("Metallic / Roughness"))
			BeginCreate(
				CreateAssetKind::MaterialMetallicRoughness);
		if (ImGui::MenuItem("Specular / Glossiness"))
			BeginCreate(
				CreateAssetKind::MaterialSpecularGlossiness);
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Animation State Machine"))
		BeginCreate(CreateAssetKind::StateMachine);

	if (ImGui::BeginMenu("Procedural Mesh"))
	{
		if (ImGui::MenuItem("Plane"))
			BeginCreate(CreateAssetKind::MeshPlane);
		if (ImGui::MenuItem("Cube"))
			BeginCreate(CreateAssetKind::MeshCube);
		if (ImGui::MenuItem("Sphere"))
			BeginCreate(CreateAssetKind::MeshSphere);
		if (ImGui::MenuItem("Cylinder"))
			BeginCreate(CreateAssetKind::MeshCylinder);
		if (ImGui::MenuItem("Cone"))
			BeginCreate(CreateAssetKind::MeshCone);
		if (ImGui::MenuItem("Torus"))
			BeginCreate(CreateAssetKind::MeshTorus);
		ImGui::EndMenu();
	}
}

void PanelAssets::DrawCreateDialog()
{
	bool focusName = false;
	if (openCreateDialog_)
	{
		EGE::EditorDialog::Open("Create Asset");
		openCreateDialog_ = false;
		focusName = true;
	}

	if (!EGE::EditorDialog::Begin(
			"Create Asset",
			ImVec2(430.0f, 0.0f),
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::TextColored(
		ImVec4(0.42f, 0.76f, 0.98f, 1.0f),
		"%s",
		CreateTypeName(createKind_));
	ImGui::Separator();
	ImGui::Spacing();
	ImGui::TextDisabled("Name");
	if (focusName)
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(-1.0f);
	const bool submitName = ImGui::InputText(
			"##CreateAssetName",
			createName_,
			sizeof(createName_),
			ImGuiInputTextFlags_EnterReturnsTrue);
	const bool canCreate = IsValidAssetName(createName_);
	if (submitName && canCreate)
	{
		CreatePendingAsset();
		if (createKind_ == CreateAssetKind::None)
			ImGui::CloseCurrentPopup();
	}
	if (!canCreate)
	{
		ImGui::TextColored(
			ImVec4(0.95f, 0.58f, 0.32f, 1.0f),
			"Enter a valid file name.");
	}

	const std::filesystem::path destination =
		browser_.GetAssetsRoot() / browser_.GetCurrentPath();
	ImGui::Spacing();
	ImGui::TextDisabled("Destination");
	ImGui::TextWrapped("%s", destination.string().c_str());

	switch (createKind_)
	{
	case CreateAssetKind::MeshPlane:
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("GEOMETRY");
		ImGui::DragFloat(
			"Width", &meshCreation_.width, 0.1f, 0.01f, 10000.0f);
		ImGui::DragFloat(
			"Height", &meshCreation_.height, 0.1f, 0.01f, 10000.0f);
		ImGui::DragInt("Slices", &meshCreation_.slices, 1.0f, 3, 256);
		ImGui::DragInt("Stacks", &meshCreation_.stacks, 1.0f, 1, 256);
		break;
	case CreateAssetKind::MeshCube:
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("GEOMETRY");
		ImGui::DragFloat(
			"Size", &meshCreation_.width, 0.1f, 0.01f, 10000.0f);
		break;
	case CreateAssetKind::MeshSphere:
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("GEOMETRY");
		ImGui::DragFloat(
			"Radius", &meshCreation_.radius, 0.1f, 0.01f, 10000.0f);
		ImGui::DragInt("Slices", &meshCreation_.slices, 1.0f, 3, 256);
		ImGui::DragInt("Stacks", &meshCreation_.stacks, 1.0f, 1, 256);
		break;
	case CreateAssetKind::MeshCylinder:
	case CreateAssetKind::MeshCone:
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("GEOMETRY");
		ImGui::DragFloat(
			"Height", &meshCreation_.height, 0.1f, 0.01f, 10000.0f);
		ImGui::DragFloat(
			"Radius", &meshCreation_.radius, 0.1f, 0.01f, 10000.0f);
		ImGui::DragInt("Slices", &meshCreation_.slices, 1.0f, 3, 256);
		ImGui::DragInt("Stacks", &meshCreation_.stacks, 1.0f, 1, 256);
		break;
	case CreateAssetKind::MeshTorus:
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextDisabled("GEOMETRY");
		ImGui::DragFloat(
			"Inner radius", &meshCreation_.innerRadius,
			0.05f, 0.01f, 10000.0f);
		ImGui::DragFloat(
			"Outer radius", &meshCreation_.outerRadius,
			0.1f, 0.01f, 10000.0f);
		ImGui::DragInt("Slices", &meshCreation_.slices, 1.0f, 3, 256);
		ImGui::DragInt("Stacks", &meshCreation_.stacks, 1.0f, 1, 256);
		break;
	default:
		break;
	}

	if (!errorMessage_.empty())
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(
			ImGuiCol_Text,
			ImVec4(0.96f, 0.43f, 0.43f, 1.0f));
		ImGui::TextWrapped("%s", errorMessage_.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::Separator();
	const float buttonWidth = 96.0f;
	ImGui::SetCursorPosX(
		std::max(
			ImGui::GetCursorPosX(),
			ImGui::GetWindowWidth() - buttonWidth * 2.0f - 24.0f));
	if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0.0f)))
	{
		createKind_ = CreateAssetKind::None;
		errorMessage_.clear();
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (!canCreate)
	{
		ImGui::PushStyleVar(
			ImGuiStyleVar_Alpha,
			ImGui::GetStyle().Alpha * 0.45f);
	}
	const bool createPressed =
		ImGui::Button("Create", ImVec2(buttonWidth, 0.0f));
	if (!canCreate)
		ImGui::PopStyleVar();
	if (createPressed && canCreate)
	{
		CreatePendingAsset();
		if (createKind_ == CreateAssetKind::None)
			ImGui::CloseCurrentPopup();
	}
	if (ImGui::IsKeyPressed(
			ImGui::GetKeyIndex(ImGuiKey_Escape), false))
	{
		createKind_ = CreateAssetKind::None;
		errorMessage_.clear();
		ImGui::CloseCurrentPopup();
	}

	EGE::EditorDialog::End();
}

void PanelAssets::BeginCreate(CreateAssetKind kind)
{
	createKind_ = kind;
	openCreateDialog_ = true;
	errorMessage_.clear();
	meshCreation_ = {};

	const char* baseName = "New Asset";
	switch (kind)
	{
	case CreateAssetKind::Folder: baseName = "New Folder"; break;
	case CreateAssetKind::Scene: baseName = "New Scene"; break;
	case CreateAssetKind::AngelScript: baseName = "New Script"; break;
	case CreateAssetKind::MaterialMetallicRoughness:
	case CreateAssetKind::MaterialSpecularGlossiness:
		baseName = "New Material";
		break;
	case CreateAssetKind::StateMachine:
		baseName = "New State Machine";
		break;
	case CreateAssetKind::MeshPlane: baseName = "New Plane"; break;
	case CreateAssetKind::MeshCube: baseName = "New Cube"; break;
	case CreateAssetKind::MeshSphere: baseName = "New Sphere"; break;
	case CreateAssetKind::MeshCylinder: baseName = "New Cylinder"; break;
	case CreateAssetKind::MeshCone: baseName = "New Cone"; break;
	case CreateAssetKind::MeshTorus: baseName = "New Torus"; break;
	case CreateAssetKind::None: break;
	}

	const std::string uniqueName =
		MakeUniqueCreateName(baseName, CreateExtension(kind));
	std::snprintf(
		createName_,
		sizeof(createName_),
		"%s",
		uniqueName.c_str());
}

void PanelAssets::CreatePendingAsset()
{
	errorMessage_.clear();
	if (!IsValidAssetName(createName_))
	{
		errorMessage_ =
			"Use a non-empty file name without path or reserved characters.";
		return;
	}

	if (createKind_ == CreateAssetKind::Folder)
	{
		CreateFolder();
		return;
	}

	const std::string sourcePath = BuildCreateSourcePath();
	if (createKind_ == CreateAssetKind::Scene)
	{
		if (!App->level->CreateEmptySceneAsset(
				sourcePath.c_str(),
				createName_,
				&errorMessage_))
		{
			if (errorMessage_.empty())
				errorMessage_ = "The scene could not be created.";
			return;
		}
		FinishAssetCreation(sourcePath, "Scene");
		return;
	}

	if (createKind_ == CreateAssetKind::AngelScript)
	{
		const EGE::ScriptAssetCreationResult result =
			EGE::ScriptAsset::Create(
				browser_.GetProjectRoot(),
				sourcePath,
				createName_);
		if (!result)
		{
			errorMessage_ = result.error;
			return;
		}
		FinishAssetCreation(result.sourcePath, "AngelScript");
		return;
	}

	ModuleResources::AssetCreationResult result;
	switch (createKind_)
	{
	case CreateAssetKind::MaterialMetallicRoughness:
		result = App->resources->CreateMaterialAsset(
			sourcePath.c_str(),
			createName_,
			ModuleResources::MaterialAssetWorkflow::MetallicRoughness);
		break;
	case CreateAssetKind::MaterialSpecularGlossiness:
		result = App->resources->CreateMaterialAsset(
			sourcePath.c_str(),
			createName_,
			ModuleResources::MaterialAssetWorkflow::SpecularGlossiness);
		break;
	case CreateAssetKind::StateMachine:
		result = App->resources->CreateStateMachineAsset(
			sourcePath.c_str(),
			createName_);
		break;
	case CreateAssetKind::MeshPlane:
	case CreateAssetKind::MeshCube:
	case CreateAssetKind::MeshSphere:
	case CreateAssetKind::MeshCylinder:
	case CreateAssetKind::MeshCone:
	case CreateAssetKind::MeshTorus:
	{
		ModuleResources::ProceduralMeshSettings settings;
		settings.width = meshCreation_.width;
		settings.height = meshCreation_.height;
		settings.radius = meshCreation_.radius;
		settings.innerRadius = meshCreation_.innerRadius;
		settings.outerRadius = meshCreation_.outerRadius;
		settings.slices =
			static_cast<unsigned int>(std::max(0, meshCreation_.slices));
		settings.stacks =
			static_cast<unsigned int>(std::max(0, meshCreation_.stacks));

		switch (createKind_)
		{
		case CreateAssetKind::MeshPlane:
			settings.shape =
				ModuleResources::ProceduralMeshShape::Plane;
			break;
		case CreateAssetKind::MeshCube:
			settings.shape =
				ModuleResources::ProceduralMeshShape::Cube;
			break;
		case CreateAssetKind::MeshSphere:
			settings.shape =
				ModuleResources::ProceduralMeshShape::Sphere;
			break;
		case CreateAssetKind::MeshCylinder:
			settings.shape =
				ModuleResources::ProceduralMeshShape::Cylinder;
			break;
		case CreateAssetKind::MeshCone:
			settings.shape =
				ModuleResources::ProceduralMeshShape::Cone;
			break;
		case CreateAssetKind::MeshTorus:
			settings.shape =
				ModuleResources::ProceduralMeshShape::Torus;
			break;
		default:
			break;
		}

		result = App->resources->CreateProceduralMeshAsset(
			sourcePath.c_str(),
			createName_,
			settings);
		break;
	}
	default:
		errorMessage_ = "This asset type cannot be created.";
		return;
	}

	if (!result)
	{
		errorMessage_ = result.error.empty()
			? "The asset could not be created."
			: result.error;
		return;
	}

	const std::string createdType = CreateTypeName(createKind_);
	FinishAssetCreation(result.sourcePath, createdType);
}

void PanelAssets::FinishAssetCreation(
	const std::string& sourcePath,
	const std::string& typeName)
{
	Refresh();
	selection_.clear();
	selection_.insert(sourcePath);
	if (const EGE::AssetEntry* entry =
			browser_.FindBySourcePath(sourcePath))
	{
		SelectInInspector(*entry);
	}
	statusMessage_ = typeName + " created.";
	createKind_ = CreateAssetKind::None;
}

void PanelAssets::CreateFolder()
{
	const std::filesystem::path folder =
		browser_.GetAssetsRoot() /
		browser_.GetCurrentPath() /
		createName_;
	std::error_code fileError;
	if (!std::filesystem::create_directory(folder, fileError))
	{
		errorMessage_ = fileError
			? "The folder could not be created: " + fileError.message()
			: "A folder with this name already exists.";
		return;
	}

	const std::filesystem::path relative =
		browser_.GetCurrentPath() / createName_;
	Refresh();
	selection_.clear();
	const std::string sourcePath =
		"Assets/" + relative.generic_string();
	selection_.insert(sourcePath);
	if (const EGE::AssetEntry* entry =
			browser_.FindBySourcePath(sourcePath))
	{
		SelectInInspector(*entry);
	}
	statusMessage_ = "Folder created.";
	createKind_ = CreateAssetKind::None;
}

std::string PanelAssets::BuildCreateSourcePath() const
{
	const std::filesystem::path relative =
		browser_.GetCurrentPath() /
		(std::string(createName_) + CreateExtension(createKind_));
	return "Assets/" + relative.generic_string();
}

std::string PanelAssets::MakeUniqueCreateName(
	const char* baseName,
	const char* extension) const
{
	const std::filesystem::path directory =
		browser_.GetAssetsRoot() / browser_.GetCurrentPath();
	std::string candidate = baseName;
	for (unsigned int suffix = 1;
		std::filesystem::exists(
			directory / (candidate + extension));
		++suffix)
	{
		candidate = std::string(baseName) + " " +
			std::to_string(suffix);
	}
	return candidate;
}

bool PanelAssets::IsValidAssetName(const char* name)
{
	if (!name || name[0] == '\0')
		return false;
	const std::string value(name);
	if (value == "." || value == ".." ||
		value.back() == '.' || value.back() == ' ')
	{
		return false;
	}
	constexpr const char* invalid = "<>:\"/\\|?*";
	return value.find_first_of(invalid) == std::string::npos &&
		std::none_of(
			value.begin(),
			value.end(),
			[](unsigned char character)
			{
				return character < 32;
			});
}

const char* PanelAssets::CreateExtension(CreateAssetKind kind)
{
	switch (kind)
	{
	case CreateAssetKind::Scene:
		return ".eduscene";
	case CreateAssetKind::AngelScript:
		return ".as";
	case CreateAssetKind::MaterialMetallicRoughness:
	case CreateAssetKind::MaterialSpecularGlossiness:
		return ".edumaterial.json";
	case CreateAssetKind::StateMachine:
		return ".edustates.json";
	case CreateAssetKind::MeshPlane:
	case CreateAssetKind::MeshCube:
	case CreateAssetKind::MeshSphere:
	case CreateAssetKind::MeshCylinder:
	case CreateAssetKind::MeshCone:
	case CreateAssetKind::MeshTorus:
		return ".edumesh.json";
	default:
		return "";
	}
}

const char* PanelAssets::CreateTypeName(CreateAssetKind kind)
{
	switch (kind)
	{
	case CreateAssetKind::Folder: return "Folder";
	case CreateAssetKind::Scene: return "Scene";
	case CreateAssetKind::AngelScript: return "AngelScript";
	case CreateAssetKind::MaterialMetallicRoughness:
		return "Metallic / Roughness Material";
	case CreateAssetKind::MaterialSpecularGlossiness:
		return "Specular / Glossiness Material";
	case CreateAssetKind::StateMachine:
		return "Animation State Machine";
	case CreateAssetKind::MeshPlane: return "Procedural Plane";
	case CreateAssetKind::MeshCube: return "Procedural Cube";
	case CreateAssetKind::MeshSphere: return "Procedural Sphere";
	case CreateAssetKind::MeshCylinder: return "Procedural Cylinder";
	case CreateAssetKind::MeshCone: return "Procedural Cone";
	case CreateAssetKind::MeshTorus: return "Procedural Torus";
	case CreateAssetKind::None: return "Asset";
	}
	return "Asset";
}

void PanelAssets::DrawImportMenu()
{
	if (!ImGui::BeginPopup("AssetImportMenu"))
		return;

	DrawImportOptions();
	ImGui::EndPopup();
}

void PanelAssets::DrawImportOptions()
{
	if (ImGui::MenuItem("Model..."))
		OpenImportDialog(Resource::model);
	if (ImGui::MenuItem("Texture..."))
		OpenImportDialog(Resource::texture);
	if (ImGui::MenuItem("Audio..."))
		OpenImportDialog(Resource::audio);
	if (ImGui::MenuItem("Animation clip..."))
		OpenImportDialog(Resource::animation);
}

void PanelAssets::EnsureProject()
{
	if (!App->GetActiveProject())
	{
		browser_.Reset();
		return;
	}

	const std::filesystem::path& activeRoot =
		App->fs->GetProjectRoot();
	if (browser_.IsOpen() &&
		browser_.GetProjectRoot() == activeRoot)
	{
		return;
	}

	if (!browser_.OpenProject(activeRoot, errorMessage_))
		return;

	history_.assign(1, {});
	historyIndex_ = 0;
	selection_.clear();
	RebuildImportIndex();
	statusMessage_ =
		std::to_string(browser_.GetAssetCount()) + " assets indexed.";
}

void PanelAssets::Refresh()
{
	errorMessage_.clear();
	if (!browser_.Refresh(errorMessage_))
		return;
	RebuildImportIndex();
	if (const EGE::EditorAssetSelection* selected =
			std::get_if<EGE::EditorAssetSelection>(
				&App->editor->GetSelection());
		selected &&
		!browser_.FindBySourcePath(selected->sourcePath))
	{
		const std::string removedPath = selected->sourcePath;
		App->editor->ClearSelected();
		selection_.erase(removedPath);
	}
	statusMessage_ =
		std::to_string(browser_.GetAssetCount()) + " assets refreshed.";
}

void PanelAssets::RebuildImportIndex()
{
	importIndex_.clear();
	for (int typeValue = Resource::model;
		typeValue < Resource::unknown;
		++typeValue)
	{
		const Resource::Type type =
			static_cast<Resource::Type>(typeValue);
		std::vector<const Resource*> resources;
		App->resources->GatherResourceType(resources, type);
		for (const Resource* resource : resources)
		{
			if (!resource || !resource->GetFile() ||
				resource->GetFile()[0] == '\0')
			{
				continue;
			}

			const std::string key =
				NormalizeAssetKey(resource->GetFile());
			if (!key.starts_with("assets/"))
				continue;

			ImportInfo& info = importIndex_[key];
			++info.resourceCount;
			info.resourceUids.push_back(resource->GetUID());
			if (info.primaryUid == 0 ||
				type == Resource::model ||
				(info.primaryType != Resource::model &&
					type == Resource::texture))
			{
				info.primaryUid = resource->GetUID();
				info.primaryType = type;
			}
		}
	}

	if (const EGE::EditorAssetSelection* selected =
			std::get_if<EGE::EditorAssetSelection>(
				&App->editor->GetSelection()))
	{
		const std::string selectedPath = selected->sourcePath;
		if (const EGE::AssetEntry* entry =
				browser_.FindBySourcePath(selectedPath))
		{
			SelectInInspector(*entry);
		}
	}
}

void PanelAssets::NavigateTo(
	const std::filesystem::path& directory,
	bool addToHistory)
{
	if (!browser_.NavigateTo(directory))
		return;

	if (addToHistory)
	{
		if (!history_.empty() &&
			history_[historyIndex_] == browser_.GetCurrentPath())
		{
			return;
		}
		if (!history_.empty())
		{
			history_.erase(
				history_.begin() +
					static_cast<std::ptrdiff_t>(historyIndex_ + 1),
				history_.end());
		}
		history_.push_back(browser_.GetCurrentPath());
		historyIndex_ = history_.size() - 1;
	}

	selectionAnchor_ = std::numeric_limits<std::size_t>::max();
}

void PanelAssets::NavigateBackward()
{
	if (historyIndex_ == 0)
		return;
	--historyIndex_;
	NavigateTo(history_[historyIndex_], false);
}

void PanelAssets::NavigateForward()
{
	if (history_.empty() || historyIndex_ + 1 >= history_.size())
		return;
	++historyIndex_;
	NavigateTo(history_[historyIndex_], false);
}

void PanelAssets::CommitPendingNavigation()
{
	if (!pendingNavigation_)
		return;
	NavigateTo(*pendingNavigation_);
	pendingNavigation_.reset();
}

void PanelAssets::HandleSelection(
	const std::vector<const EGE::AssetEntry*>& entries,
	std::size_t index,
	bool fromContextMenu)
{
	if (index >= entries.size())
		return;

	const ImGuiIO& input = ImGui::GetIO();
	const std::string& selectedPath = entries[index]->sourcePath;
	if (fromContextMenu && selection_.contains(selectedPath))
	{
		SyncInspectorSelection(entries, index);
		return;
	}

	if (input.KeyShift &&
		selectionAnchor_ != std::numeric_limits<std::size_t>::max())
	{
		if (!input.KeyCtrl)
			selection_.clear();
		const std::size_t first = std::min(selectionAnchor_, index);
		const std::size_t last = std::max(selectionAnchor_, index);
		for (std::size_t current = first; current <= last; ++current)
			selection_.insert(entries[current]->sourcePath);
		SyncInspectorSelection(entries, index);
		return;
	}

	if (input.KeyCtrl && !fromContextMenu)
	{
		if (selection_.contains(selectedPath))
			selection_.erase(selectedPath);
		else
			selection_.insert(selectedPath);
	}
	else
	{
		selection_.clear();
		selection_.insert(selectedPath);
	}
	selectionAnchor_ = index;
	SyncInspectorSelection(entries, index);
}

void PanelAssets::SyncInspectorSelection(
	const std::vector<const EGE::AssetEntry*>& entries,
	std::size_t preferredIndex)
{
	if (preferredIndex < entries.size() &&
		selection_.contains(entries[preferredIndex]->sourcePath))
	{
		SelectInInspector(*entries[preferredIndex]);
		return;
	}

	const auto iterator = std::find_if(
		entries.begin(),
		entries.end(),
		[this](const EGE::AssetEntry* entry)
		{
			return selection_.contains(entry->sourcePath);
		});
	if (iterator != entries.end())
	{
		SelectInInspector(**iterator);
		return;
	}

	if (std::holds_alternative<EGE::EditorAssetSelection>(
			App->editor->GetSelection()))
	{
		App->editor->ClearSelected();
	}
}

void PanelAssets::SelectInInspector(const EGE::AssetEntry& entry)
{
	EGE::EditorAssetSelection selection;
	selection.sourcePath = entry.sourcePath;
	selection.kind = entry.kind;
	selection.directory = entry.directory;
	if (const ImportInfo* info = FindImportInfo(entry))
	{
		selection.primaryResource =
			ResolvePrimaryResource(entry, *info);
		selection.linkedResources = info->resourceUids;
	}
	App->editor->SetSelected(std::move(selection));
}

void PanelAssets::OpenScene(const EGE::AssetEntry& entry)
{
	const std::filesystem::path scenePath =
		browser_.GetAssetsRoot() / entry.relativePath;
	if (App->editor->OpenSceneAsset(scenePath))
		statusMessage_ = entry.name + " opened.";
}

void PanelAssets::OpenAssetEditor(const EGE::AssetEntry& entry)
{
	const ImportInfo* info = FindImportInfo(entry);
	if (!info)
		return;

	if (entry.kind == EGE::AssetKind::Model)
		return;

	const UID primaryUid = ResolvePrimaryResource(entry, *info);
	const Resource* primaryResource =
		App->resources->Get(primaryUid);
	if (primaryResource &&
		primaryResource->GetType() == Resource::texture)
	{
		if (ResourceTexture* texture =
				App->resources->GetTexture(primaryUid))
		{
			texturePreview_.Open(nullptr, texture);
		}
		return;
	}
	if (entry.kind != EGE::AssetKind::Material &&
		entry.kind != EGE::AssetKind::StateMachine &&
		entry.kind != EGE::AssetKind::Mesh)
	{
		return;
	}
	if (!primaryResource ||
		(primaryResource->GetType() != Resource::material &&
		 primaryResource->GetType() != Resource::state_machine &&
		 primaryResource->GetType() != Resource::mesh))
	{
		return;
	}

	EGE::EditorAssetSelection selection;
	selection.sourcePath = entry.sourcePath;
	selection.kind = entry.kind;
	selection.directory = entry.directory;
	selection.primaryResource = primaryUid;
	selection.linkedResources = info->resourceUids;
	App->editor->OpenAssetEditor(selection);
}

void PanelAssets::OpenScriptInVsCode(const EGE::AssetEntry& entry)
{
	const std::filesystem::path filePath =
		browser_.GetAssetsRoot() / entry.relativePath;
	if (!EGE::OpenVsCode(browser_.GetProjectRoot(), filePath, errorMessage_))
		return;

	statusMessage_ = entry.name + " opened in Visual Studio Code.";
}

void PanelAssets::HandleAssetInteractions(
	const EGE::AssetEntry& entry,
	const std::vector<const EGE::AssetEntry*>& entries,
	std::size_t index)
{
	const bool hovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		HandleSelection(entries, index, false);
	if (hovered &&
		ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		if (entry.directory)
			pendingNavigation_ = entry.relativePath;
		else if (entry.kind == EGE::AssetKind::Scene)
			OpenScene(entry);
		else if (entry.kind == EGE::AssetKind::Script)
			OpenScriptInVsCode(entry);
		else
			OpenAssetEditor(entry);
	}
	if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
		HandleSelection(entries, index, true);

	if (!entry.directory && ImGui::BeginDragDropSource())
	{
		ImGui::SetDragDropPayload(
			"EGE_ASSET_PATH",
			entry.sourcePath.c_str(),
			entry.sourcePath.size() + 1);
		ImGui::Text("%s", entry.name.c_str());
		ImGui::TextDisabled(
			"%s",
			EGE::AssetBrowserModel::GetKindName(entry.kind));
		ImGui::EndDragDropSource();
	}

	if (ImGui::BeginPopupContextItem("AssetContextMenu"))
	{
		DrawAssetContextMenu(entry);
		ImGui::EndPopup();
	}

	if (hovered)
	{
		ImGui::BeginTooltip();
		ImGui::Text("%s", entry.name.c_str());
		ImGui::TextDisabled("%s", entry.sourcePath.c_str());
		ImGui::Text(
			"Type: %s",
			EGE::AssetBrowserModel::GetKindName(entry.kind));
		if (!entry.directory)
			ImGui::Text("Size: %s", FormatFileSize(entry.size).c_str());
		if (const ImportInfo* info = FindImportInfo(entry))
		{
			ImGui::Text(
				"Imported resources: %zu",
				info->resourceCount);
		}
		ImGui::EndTooltip();
	}
}

void PanelAssets::OpenImportDialog(Resource::Type type)
{
	waitingToImport_ = type;
	std::vector<const char*> filters;
	switch (type)
	{
	case Resource::model:
		filters = {".fbx", ".dae", ".gltf", ".glb"};
		break;
	case Resource::texture:
		filters = {
			".png", ".jpg", ".jpeg", ".tga", ".tif", ".dds", ".hdr"};
		break;
	case Resource::audio:
		filters = {".wav", ".ogg"};
		break;
	case Resource::animation:
		filters = {".fbx", ".dae", ".gltf", ".glb"};
		break;
	default:
		break;
	}
	fileDialog_.SetTypeFilters(filters);
	fileDialog_.SetPwd(
		browser_.GetAssetsRoot() /
		browser_.GetCurrentPath());
	fileDialog_.Open();
}

bool PanelAssets::PrepareImportSource(
	const std::filesystem::path& selectedPath,
	std::string& projectSourcePath,
	bool& copiedIntoProject)
{
	projectSourcePath.clear();
	copiedIntoProject = false;
	errorMessage_.clear();

	std::error_code fileError;
	const std::filesystem::path selected =
		std::filesystem::weakly_canonical(selectedPath, fileError);
	if (fileError ||
		!std::filesystem::is_regular_file(selected, fileError))
	{
		errorMessage_ =
			"The selected import source is not a readable file.";
		return false;
	}

	fileError.clear();
	const std::filesystem::path projectRoot =
		std::filesystem::weakly_canonical(
			browser_.GetProjectRoot(), fileError);
	if (fileError)
	{
		errorMessage_ = "The active project directory is unavailable.";
		return false;
	}

	fileError.clear();
	const std::filesystem::path assetsRoot =
		std::filesystem::weakly_canonical(
			browser_.GetAssetsRoot(), fileError);
	if (fileError)
	{
		errorMessage_ = "The active project's Assets folder is unavailable.";
		return false;
	}

	std::filesystem::path projectAsset = selected;
	if (!IsPathInside(selected, assetsRoot))
	{
		const std::filesystem::path destinationDirectory =
			(assetsRoot / browser_.GetCurrentPath()).lexically_normal();
		if (!IsPathInside(destinationDirectory, assetsRoot) ||
			!std::filesystem::is_directory(destinationDirectory, fileError))
		{
			errorMessage_ =
				"The current Assets folder is not available.";
			return false;
		}

		projectAsset = destinationDirectory / selected.filename();
		if (std::filesystem::exists(projectAsset, fileError))
		{
			const std::string stem = selected.stem().string();
			const std::string extension = selected.extension().string();
			for (std::uint32_t suffix = 1;; ++suffix)
			{
				projectAsset =
					destinationDirectory /
					(stem + " (" + std::to_string(suffix) + ")" +
					 extension);
				fileError.clear();
				if (!std::filesystem::exists(projectAsset, fileError))
					break;
				if (fileError)
				{
					errorMessage_ =
						"Could not choose a destination for the asset.";
					return false;
				}
			}
		}
		else if (fileError)
		{
			errorMessage_ =
				"Could not inspect the destination Assets folder.";
			return false;
		}

		fileError.clear();
		if (!std::filesystem::copy_file(
				selected,
				projectAsset,
				std::filesystem::copy_options::none,
				fileError))
		{
			errorMessage_ =
				"Could not copy the asset into the project: " +
				fileError.message();
			return false;
		}
		copiedIntoProject = true;
	}

	const std::filesystem::path relativePath =
		projectAsset.lexically_relative(projectRoot);
	if (relativePath.empty() ||
		relativePath.is_absolute() ||
		!IsPathInside(projectAsset, assetsRoot))
	{
		errorMessage_ =
			"The import source could not be resolved inside the project.";
		return false;
	}

	projectSourcePath = relativePath.generic_string();
	return true;
}

void PanelAssets::ImportResource(
	const std::string& sourcePath,
	Resource::Type type)
{
	errorMessage_.clear();
	switch (type)
	{
	case Resource::model:
		modelDialog_.Open(sourcePath);
		break;
	case Resource::texture:
		textureDialog_.Open(sourcePath);
		break;
	case Resource::animation:
	{
		std::string userName =
			std::filesystem::path(sourcePath).stem().string();
		animationDialog_.Open(sourcePath, userName);
		break;
	}
	case Resource::audio:
		audioDialog_.Open(sourcePath);
		break;
	default:
		errorMessage_ = "This asset type does not have an importer yet.";
		break;
	}
}

Resource::Type PanelAssets::ResourceTypeFor(
	const EGE::AssetEntry& entry) const
{
	switch (entry.kind)
	{
	case EGE::AssetKind::Model:
		return Resource::model;
	case EGE::AssetKind::Texture:
		return Resource::texture;
	case EGE::AssetKind::Audio:
		return Resource::audio;
	case EGE::AssetKind::Animation:
		return Resource::animation;
	default:
		return Resource::unknown;
	}
}

const PanelAssets::ImportInfo* PanelAssets::FindImportInfo(
	const EGE::AssetEntry& entry) const
{
	const auto iterator =
		importIndex_.find(NormalizeAssetKey(entry.sourcePath));
	return iterator == importIndex_.end()
		? nullptr
		: &iterator->second;
}

UID PanelAssets::ResolvePrimaryResource(
	const EGE::AssetEntry& entry,
	const ImportInfo& info) const
{
	Resource::Type expectedType = ResourceTypeFor(entry);
	switch (entry.kind)
	{
	case EGE::AssetKind::Material:
		expectedType = Resource::material;
		break;
	case EGE::AssetKind::Mesh:
		expectedType = Resource::mesh;
		break;
	case EGE::AssetKind::StateMachine:
		expectedType = Resource::state_machine;
		break;
	default:
		break;
	}

	if (expectedType != Resource::unknown)
	{
		for (UID uid : info.resourceUids)
		{
			const Resource* resource = App->resources->Get(uid);
			if (resource && resource->GetType() == expectedType)
				return uid;
		}
		return 0;
	}
	return info.primaryUid;
}

std::string PanelAssets::NormalizeAssetKey(std::string path)
{
	std::replace(path.begin(), path.end(), '\\', '/');
	while (!path.empty() && path.front() == '/')
		path.erase(path.begin());
	std::transform(
		path.begin(), path.end(), path.begin(),
		[](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
	return path;
}

std::string PanelAssets::ShortenText(
	const std::string& text,
	float availableWidth)
{
	if (ImGui::CalcTextSize(text.c_str()).x <= availableWidth)
		return text;

	std::string shortened = text;
	while (shortened.size() > 3)
	{
		shortened.pop_back();
		const std::string candidate = shortened + "...";
		if (ImGui::CalcTextSize(candidate.c_str()).x <= availableWidth)
			return candidate;
	}
	return "...";
}

std::string PanelAssets::FormatFileSize(std::uintmax_t bytes)
{
	constexpr double Kilobyte = 1024.0;
	constexpr double Megabyte = Kilobyte * 1024.0;
	constexpr double Gigabyte = Megabyte * 1024.0;
	char buffer[32] = {};
	if (bytes >= static_cast<std::uintmax_t>(Gigabyte))
		std::snprintf(
			buffer, sizeof(buffer), "%.1f GB", bytes / Gigabyte);
	else if (bytes >= static_cast<std::uintmax_t>(Megabyte))
		std::snprintf(
			buffer, sizeof(buffer), "%.1f MB", bytes / Megabyte);
	else if (bytes >= static_cast<std::uintmax_t>(Kilobyte))
		std::snprintf(
			buffer, sizeof(buffer), "%.1f KB", bytes / Kilobyte);
	else
		std::snprintf(
			buffer, sizeof(buffer), "%llu B",
			static_cast<unsigned long long>(bytes));
	return buffer;
}

ImVec4 PanelAssets::KindColor(EGE::AssetKind kind)
{
	switch (kind)
	{
	case EGE::AssetKind::Folder:
		return ImVec4(0.92f, 0.68f, 0.27f, 1.0f);
	case EGE::AssetKind::Scene:
		return ImVec4(0.36f, 0.82f, 0.55f, 1.0f);
	case EGE::AssetKind::Model:
		return ImVec4(0.66f, 0.48f, 0.95f, 1.0f);
	case EGE::AssetKind::Mesh:
		return ImVec4(0.48f, 0.62f, 0.96f, 1.0f);
	case EGE::AssetKind::Texture:
		return ImVec4(0.28f, 0.72f, 0.92f, 1.0f);
	case EGE::AssetKind::Material:
		return ImVec4(0.95f, 0.55f, 0.28f, 1.0f);
	case EGE::AssetKind::Audio:
		return ImVec4(0.93f, 0.42f, 0.68f, 1.0f);
	case EGE::AssetKind::Animation:
		return ImVec4(0.39f, 0.82f, 0.72f, 1.0f);
	case EGE::AssetKind::StateMachine:
		return ImVec4(0.86f, 0.47f, 0.91f, 1.0f);
	case EGE::AssetKind::Script:
		return ImVec4(0.37f, 0.78f, 0.56f, 1.0f);
	case EGE::AssetKind::Shader:
		return ImVec4(0.32f, 0.58f, 0.96f, 1.0f);
	case EGE::AssetKind::Font:
		return ImVec4(0.80f, 0.84f, 0.92f, 1.0f);
	case EGE::AssetKind::Data:
		return ImVec4(0.52f, 0.63f, 0.75f, 1.0f);
	case EGE::AssetKind::Unknown:
		return ImVec4(0.45f, 0.49f, 0.57f, 1.0f);
	}
	return ImVec4(0.45f, 0.49f, 0.57f, 1.0f);
}

const char* PanelAssets::KindGlyph(EGE::AssetKind kind)
{
	switch (kind)
	{
	case EGE::AssetKind::Scene: return "SCN";
	case EGE::AssetKind::Model: return "3D";
	case EGE::AssetKind::Mesh: return "MESH";
	case EGE::AssetKind::Texture: return "TEX";
	case EGE::AssetKind::Material: return "MAT";
	case EGE::AssetKind::Audio: return "AUD";
	case EGE::AssetKind::Animation: return "ANIM";
	case EGE::AssetKind::StateMachine: return "FSM";
	case EGE::AssetKind::Script: return "AS";
	case EGE::AssetKind::Shader: return "FX";
	case EGE::AssetKind::Font: return "FONT";
	case EGE::AssetKind::Data: return "DATA";
	case EGE::AssetKind::Folder: return "";
	case EGE::AssetKind::Unknown: return "FILE";
	}
	return "FILE";
}
