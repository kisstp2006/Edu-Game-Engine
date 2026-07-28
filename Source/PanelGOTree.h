#ifndef __PANELGOTREE_H__
#define __PANELGOTREE_H__

// Editor Panel to show the full tree of game objects of the scene
#include "AssetBrowserModel.h"
#include "ImportModelDlg.h"
#include "Panel.h"

#include <filesystem>
#include <string>
#include <vector>

class GameObject;

class PanelGOTree : public Panel
{
public:
	PanelGOTree();
	virtual ~PanelGOTree();

	void Draw() override;
	void ResetProjectState();

private:

    void DrawLights();
    void DrawSkybox();
	void DrawPrefabAssetMenu();
	void DrawModelPrefabMenu();
	void DrawModelImportDialog();
	void EnsureModelAssetIndex();
	void BuildSelectionOrder(GameObject* gameObject);
	void HandleSelectionClick(GameObject* gameObject);
	std::vector<GameObject*> GetSelectionRoots(
		GameObject* context) const;
	void HandleShortcuts();
	void DrawActionDialogs();
	void DuplicateSelection(GameObject* context = nullptr);
	void RequestDeleteSelection(GameObject* context = nullptr);
	void DeletePendingSelection();
	void RequestRenameSelection(GameObject* context = nullptr);
	void FrameSelection() const;
	bool RecursiveDraw(GameObject* go);
	void CheckHover(GameObject* go);

	EGE::AssetBrowserModel modelAssetIndex_;
	std::filesystem::path indexedProjectRoot_;
	std::string modelAssetError_;
	ImportModelDlg modelImportDialog_;
	bool modelMenuWasOpen_ = false;
	std::vector<GameObject*> selectionOrder_;
	GameObject* selectionAnchor_ = nullptr;
	std::vector<uint> pendingDeleteIds_;
	uint pendingRenameId_ = 0;
	bool openDeleteDialog_ = false;
	bool openRenameDialog_ = false;
	char renameBuffer_[256] = {};

public:

	GameObject* drag = nullptr;
	GameObject* drag_candidate = nullptr;
	bool open_selected = false;
};

#endif// __PANELGOTREE_H__
