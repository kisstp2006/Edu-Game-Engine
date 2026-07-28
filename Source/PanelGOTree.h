#ifndef __PANELGOTREE_H__
#define __PANELGOTREE_H__

// Editor Panel to show the full tree of game objects of the scene
#include "AssetBrowserModel.h"
#include "ImportModelDlg.h"
#include "Panel.h"

#include <filesystem>
#include <string>

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
	void DrawModelPrefabMenu();
	void DrawModelImportDialog();
	void EnsureModelAssetIndex();
	bool RecursiveDraw(GameObject* go);
	void CheckHover(GameObject* go);

	EGE::AssetBrowserModel modelAssetIndex_;
	std::filesystem::path indexedProjectRoot_;
	std::string modelAssetError_;
	ImportModelDlg modelImportDialog_;
	bool modelMenuWasOpen_ = false;

public:

	GameObject* drag = nullptr;
	GameObject* drag_candidate = nullptr;
	bool open_selected = false;
};

#endif// __PANELGOTREE_H__
