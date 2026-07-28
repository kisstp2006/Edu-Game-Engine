#include "Globals.h"
#include "Application.h"
#include "PanelGOTree.h"
#include <imgui.h>
#include "Application.h"
#include "EditorDialog.h"
#include "ModuleLevelManager.h"
#include "LightManager.h"
#include "ModuleEditor.h"
#include "ModuleEditorCamera.h"
#include "ModuleFileSystem.h"
#include "ModuleResources.h"
#include "PanelAssets.h"
#include "ResourceModel.h"
#include "GameObject.h"

#include <list>
#include <SDL_scancode.h>
#include <string>
#include <variant>

#include <stdio.h>

#include "Leaks.h"

using namespace std;

namespace
{
	template<typename Light, typename Getter, typename Remover>
	void DrawLightCollection(
		const char* typeName,
		uint count,
		Getter getLight,
		Remover removeLight)
	{
		for (uint index = 0; index < count; ++index)
		{
			Light* light = getLight(index);
			if (!light)
				continue;

			const std::string label =
				count == 1
					? std::string(typeName)
					: std::string(typeName) + " " +
						std::to_string(index + 1);
			Light* const* selected =
				std::get_if<Light*>(&App->editor->GetSelection());

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf |
				ImGuiTreeNodeFlags_NoTreePushOnOpen |
				ImGuiTreeNodeFlags_SpanAvailWidth;
			if (selected && *selected == light)
				flags |= ImGuiTreeNodeFlags_Selected;

			ImGui::PushID(light);
			ImGui::TreeNodeEx(label.c_str(), flags);
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
				App->editor->SetSelected(light);

			bool remove = false;
			if (ImGui::BeginPopupContextItem("##LightOptions"))
			{
				remove = ImGui::MenuItem("Remove");
				ImGui::EndPopup();
			}
			ImGui::PopID();

			if (remove)
			{
				if (selected && *selected == light)
					App->editor->ClearSelected();
				removeLight(index);
				break;
			}
		}
	}
}

// ---------------------------------------------------------
PanelGOTree::PanelGOTree() : Panel("Game Objects")
{
	width = 325;
	height = 500;
	posx = 2;
	posy = 21;
}

// ---------------------------------------------------------
PanelGOTree::~PanelGOTree()
{
}

// ---------------------------------------------------------
void PanelGOTree::Draw()
{
	DrawActionDialogs();

	if (!drag &&
		drag_candidate &&
		ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f))
	{
		drag = drag_candidate;
	}
	GameObject* root = App->level->GetRoot();
	selectionOrder_.clear();
	for (GameObject* gameObject : root->childs)
		BuildSelectionOrder(gameObject);
	selectionAnchor_ = App->level->Validate(selectionAnchor_);

	//ImGui::SetNextWindowContentWidth((float) (width*2));
    //ImGui::Begin("GameObjects Hierarchy", &active, 
		//ImGuiWindowFlags_NoResize | 
		//ImGuiWindowFlags_NoFocusOnAppearing |
		//ImGuiWindowFlags_HorizontalScrollbar );

	if (ImGui::BeginMenu("Options"))
	{
		GameObject* primary = App->editor->GetPrimaryGameObject();
		const bool hasSelection = primary != nullptr;
		if (ImGui::MenuItem("New Game Object", "Ctrl+Shift+N"))
		{
			if (GameObject* gameObject =
					App->level->CreateGameObject())
			{
				App->editor->SetSelected(gameObject);
				selectionAnchor_ = gameObject;
			}
		}
		if (ImGui::MenuItem(
				"Select All", "Ctrl+A", false,
				!selectionOrder_.empty()))
		{
			App->editor->SetGameObjectSelection(
				selectionOrder_,
				selectionOrder_.empty()
					? nullptr
					: selectionOrder_.back());
		}
		if (ImGui::MenuItem(
				"Duplicate Selected", "Ctrl+D", false, hasSelection))
			DuplicateSelection();
		if (ImGui::MenuItem(
				"Rename Selected", "F2", false, hasSelection))
			RequestRenameSelection();
		if (ImGui::MenuItem(
				"Frame Selected", "F", false, hasSelection))
			FrameSelection();
		if (ImGui::MenuItem(
				"Delete Selected", "Delete", false, hasSelection))
			RequestDeleteSelection();

		ImGui::Separator();
		if (ImGui::MenuItem("Load..", nullptr, false, App->IsStop()))
			App->editor->RequestOpenScene();

		if (ImGui::MenuItem("Save..", nullptr, false, App->IsStop()))
			App->editor->RequestSaveScene(true);

		if (ImGui::BeginMenu("Load Prefab"))
		{
			DrawPrefabAssetMenu();
			DrawModelPrefabMenu();
            ImGui::EndMenu();
		}

        if(ImGui::MenuItem("New Point Light"))
            App->level->GetLightManager()->AddPointLight();

        if(ImGui::MenuItem("New Spot Light"))
            App->level->GetLightManager()->AddSpotLight();

        if(ImGui::MenuItem("New Quad Light"))
            App->level->GetLightManager()->AddQuadLight();

        if(ImGui::MenuItem("New Sphere Light"))
            App->level->GetLightManager()->AddSphereLight();

        if(ImGui::MenuItem("New Tube Light"))
            App->level->GetLightManager()->AddTubeLight();

        if (ImGui::MenuItem("New Local IBL Light"))
            App->level->GetLightManager()->AddLocalIBLLight();


		if (ImGui::MenuItem("Clear Scene", "!"))
			App->level->GetRoot()->Remove();

        if (ImGui::MenuItem("Generate Local IBLs"))
            App->level->GetLightManager()->generateIBLs();


		ImGui::EndMenu();
	}

	DrawModelImportDialog();

	HandleShortcuts();
	for (GameObject* gameObject : root->childs)
	{
		if (!gameObject->IsPendingDestroy() &&
			RecursiveDraw(gameObject))
		{
			break;
		}
	}

    DrawLights();
    DrawSkybox();

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
	{
		drag = nullptr;
		drag_candidate = nullptr;
	}

    //ImGui::End();
}

// ---------------------------------------------------------
void PanelGOTree::DrawSkybox()
{
    IBLData* const* skybox = std::get_if<IBLData*>(&App->editor->GetSelection());

    uint flags = ImGuiTreeNodeFlags_Leaf;
    if(skybox != nullptr && *skybox == App->level->GetSkyBox())
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    if(ImGui::TreeNodeEx("Skybox", flags))
    {
        if (ImGui::IsItemClicked(0)) 
        {
            App->editor->SetSelected(App->level->GetSkyBox());
        }

        ImGui::TreePop();
    }

}

// ---------------------------------------------------------
void PanelGOTree::DrawLights()
{
	LightManager* lights = App->level->GetLightManager();

	DirLight* directional = lights->GetDirLight();
	DirLight* const* selected =
		std::get_if<DirLight*>(&App->editor->GetSelection());

	ImGuiTreeNodeFlags directionalFlags =
		ImGuiTreeNodeFlags_Leaf |
		ImGuiTreeNodeFlags_NoTreePushOnOpen |
		ImGuiTreeNodeFlags_SpanAvailWidth;
	if (selected && *selected == directional)
		directionalFlags |= ImGuiTreeNodeFlags_Selected;

	ImGui::PushID(directional);
	ImGui::TreeNodeEx("Directional Light", directionalFlags);
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		App->editor->SetSelected(directional);
	ImGui::PopID();

	DrawLightCollection<PointLight>(
		"Point Light",
		lights->GetNumPointLights(),
		[lights](uint index) { return lights->GetPointLight(index); },
		[lights](uint index) { lights->RemovePointLight(index); });
	DrawLightCollection<SpotLight>(
		"Spot Light",
		lights->GetNumSpotLights(),
		[lights](uint index) { return lights->GetSpotLight(index); },
		[lights](uint index) { lights->RemoveSpotLight(index); });
	DrawLightCollection<QuadLight>(
		"Quad Light",
		lights->GetNumQuadLights(),
		[lights](uint index) { return lights->GetQuadLight(index); },
		[lights](uint index) { lights->RemoveQuadLight(index); });
	DrawLightCollection<SphereLight>(
		"Sphere Light",
		lights->GetNumSphereLights(),
		[lights](uint index) { return lights->GetSphereLight(index); },
		[lights](uint index) { lights->RemoveSphereLight(index); });
	DrawLightCollection<TubeLight>(
		"Tube Light",
		lights->GetNumTubeLights(),
		[lights](uint index) { return lights->GetTubeLight(index); },
		[lights](uint index) { lights->RemoveTubeLight(index); });
	DrawLightCollection<LocalIBLLight>(
		"Local IBL",
		lights->GetNumLocalIBLLights(),
		[lights](uint index) { return lights->GetLocalIBLLight(index); },
		[lights](uint index) { lights->RemoveLocalIBLLight(index); });
}

void PanelGOTree::ResetProjectState()
{
	modelAssetIndex_.Reset();
	indexedProjectRoot_.clear();
	modelAssetError_.clear();
	modelImportDialog_.ClearSelection();
	modelMenuWasOpen_ = false;
	selectionOrder_.clear();
	selectionAnchor_ = nullptr;
	pendingDeleteIds_.clear();
	pendingRenameId_ = 0;
	openDeleteDialog_ = false;
	openRenameDialog_ = false;
	renameBuffer_[0] = '\0';
}

void PanelGOTree::BuildSelectionOrder(GameObject* gameObject)
{
	if (!gameObject || gameObject->IsPendingDestroy())
		return;

	if (gameObject->name.find("$AssimpFbx$") == std::string::npos)
		selectionOrder_.push_back(gameObject);
	for (GameObject* child : gameObject->childs)
		BuildSelectionOrder(child);
}

void PanelGOTree::HandleSelectionClick(GameObject* gameObject)
{
	if (!gameObject)
		return;

	const ImGuiIO& input = ImGui::GetIO();
	if (input.KeyShift && selectionAnchor_)
	{
		const auto anchor = std::find(
			selectionOrder_.begin(),
			selectionOrder_.end(),
			selectionAnchor_);
		const auto current = std::find(
			selectionOrder_.begin(),
			selectionOrder_.end(),
			gameObject);
		if (anchor != selectionOrder_.end() &&
			current != selectionOrder_.end())
		{
			const auto first = std::min(anchor, current);
			const auto last = std::max(anchor, current);
			std::vector<GameObject*> selection;
			if (input.KeyCtrl)
			{
				if (const EGE::GameObjectSelection* existing =
						App->editor->GetGameObjectSelection())
				{
					selection = existing->objects;
				}
			}
			for (auto iterator = first; iterator != last + 1; ++iterator)
			{
				if (std::find(
						selection.begin(),
						selection.end(),
						*iterator) == selection.end())
				{
					selection.push_back(*iterator);
				}
			}
			App->editor->SetGameObjectSelection(
				std::move(selection), gameObject);
			return;
		}
	}

	if (input.KeyCtrl)
		App->editor->ToggleGameObjectSelection(gameObject);
	else
		App->editor->SetSelected(gameObject);
	selectionAnchor_ = gameObject;
}

std::vector<GameObject*> PanelGOTree::GetSelectionRoots(
	GameObject* context) const
{
	std::vector<GameObject*> objects;
	if (App->editor->IsGameObjectSelected(context))
	{
		if (const EGE::GameObjectSelection* selection =
				App->editor->GetGameObjectSelection())
		{
			objects = selection->objects;
		}
	}
	if (objects.empty() && context)
		objects.push_back(context);

	std::vector<GameObject*> roots;
	for (GameObject* candidate : objects)
	{
		const bool hasSelectedAncestor = std::any_of(
			objects.begin(),
			objects.end(),
			[candidate](const GameObject* other)
			{
				return candidate != other &&
					candidate->IsUnder(other);
			});
		if (!hasSelectedAncestor)
			roots.push_back(candidate);
	}
	return roots;
}

void PanelGOTree::HandleShortcuts()
{
	const ImGuiIO& input = ImGui::GetIO();
	if (!ImGui::IsWindowFocused(
			ImGuiFocusedFlags_RootAndChildWindows) ||
		input.WantTextInput ||
		ImGui::IsAnyItemActive() ||
		ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{
		return;
	}

	if (input.KeyCtrl &&
		input.KeyShift &&
		ImGui::IsKeyPressed(SDL_SCANCODE_N, false))
	{
		if (GameObject* gameObject =
				App->level->CreateGameObject())
		{
			App->editor->SetSelected(gameObject);
			selectionAnchor_ = gameObject;
		}
		return;
	}
	if (input.KeyCtrl &&
		ImGui::IsKeyPressed(SDL_SCANCODE_A, false))
	{
		App->editor->SetGameObjectSelection(
			selectionOrder_,
			selectionOrder_.empty()
				? nullptr
				: selectionOrder_.back());
		return;
	}
	if (input.KeyCtrl &&
		ImGui::IsKeyPressed(SDL_SCANCODE_D, false))
	{
		DuplicateSelection();
		return;
	}
	if (ImGui::IsKeyPressed(SDL_SCANCODE_DELETE, false))
	{
		RequestDeleteSelection();
		return;
	}
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F2, false))
	{
		RequestRenameSelection();
		return;
	}
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F, false))
	{
		FrameSelection();
		return;
	}
	if (ImGui::IsKeyPressed(SDL_SCANCODE_ESCAPE, false))
		App->editor->ClearSelected();
}

void PanelGOTree::DrawActionDialogs()
{
	if (openDeleteDialog_)
	{
		EGE::EditorDialog::Open("Delete Game Objects");
		openDeleteDialog_ = false;
	}
	if (EGE::EditorDialog::Begin(
			"Delete Game Objects",
			ImVec2(430.0f, 0.0f),
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text(
			"Delete %zu selected GameObject%s?",
			pendingDeleteIds_.size(),
			pendingDeleteIds_.size() == 1 ? "" : "s");
		ImGui::Spacing();
		ImGui::TextDisabled(
			"This removes the selection and all of its children.");
		ImGui::Spacing();
		if (ImGui::Button("Cancel"))
		{
			pendingDeleteIds_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Delete"))
		{
			DeletePendingSelection();
			ImGui::CloseCurrentPopup();
		}
		EGE::EditorDialog::End();
	}

	bool focusRename = false;
	if (openRenameDialog_)
	{
		EGE::EditorDialog::Open("Rename Game Object");
		openRenameDialog_ = false;
		focusRename = true;
	}
	if (EGE::EditorDialog::Begin(
			"Rename Game Object",
			ImVec2(430.0f, 0.0f),
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (focusRename)
			ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(-1.0f);
		const bool submit = ImGui::InputText(
			"##GameObjectName",
			renameBuffer_,
			sizeof(renameBuffer_),
			ImGuiInputTextFlags_EnterReturnsTrue);
		const bool valid = renameBuffer_[0] != '\0';
		if (!valid)
			ImGui::TextDisabled("The name cannot be empty.");
		if (ImGui::Button("Cancel"))
		{
			pendingRenameId_ = 0;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if ((ImGui::Button("Rename") || submit) && valid)
		{
			if (GameObject* gameObject =
					App->level->Find(pendingRenameId_))
			{
				gameObject->name = renameBuffer_;
			}
			pendingRenameId_ = 0;
			ImGui::CloseCurrentPopup();
		}
		EGE::EditorDialog::End();
	}
}

void PanelGOTree::DuplicateSelection(GameObject* context)
{
	if (!context)
		context = App->editor->GetPrimaryGameObject();

	std::vector<GameObject*> duplicates;
	for (GameObject* gameObject : GetSelectionRoots(context))
	{
		if (GameObject* duplicate =
				App->level->Duplicate(gameObject))
		{
			duplicates.push_back(duplicate);
		}
	}
	if (!duplicates.empty())
	{
		App->editor->SetGameObjectSelection(
			duplicates, duplicates.back());
		selectionAnchor_ = duplicates.back();
	}
}

void PanelGOTree::RequestDeleteSelection(GameObject* context)
{
	if (!context)
		context = App->editor->GetPrimaryGameObject();

	pendingDeleteIds_.clear();
	for (GameObject* gameObject : GetSelectionRoots(context))
	{
		if (gameObject)
			pendingDeleteIds_.push_back(gameObject->GetUID());
	}
	openDeleteDialog_ = !pendingDeleteIds_.empty();
}

void PanelGOTree::DeletePendingSelection()
{
	for (uint uid : pendingDeleteIds_)
	{
		if (GameObject* gameObject = App->level->Find(uid))
			gameObject->Remove();
	}
	pendingDeleteIds_.clear();
	App->editor->ClearSelected();
	selectionAnchor_ = nullptr;
	drag = nullptr;
	drag_candidate = nullptr;
}

void PanelGOTree::RequestRenameSelection(GameObject* context)
{
	GameObject* gameObject = context
		? context
		: App->editor->GetPrimaryGameObject();
	if (!gameObject)
		return;

	pendingRenameId_ = gameObject->GetUID();
	std::snprintf(
		renameBuffer_,
		sizeof(renameBuffer_),
		"%s",
		gameObject->name.c_str());
	openRenameDialog_ = true;
}

void PanelGOTree::FrameSelection() const
{
	GameObject* gameObject = App->editor->GetPrimaryGameObject();
	if (!gameObject)
		return;

	const float radius =
		gameObject->global_bbox.MinimalEnclosingSphere().r;
	App->camera->CenterOn(
		gameObject->GetGlobalPosition(),
		std::fmaxf(radius, 5.0f) * 2.0f);
}

void PanelGOTree::EnsureModelAssetIndex()
{
	if (!App->GetActiveProject())
	{
		ResetProjectState();
		return;
	}

	const std::filesystem::path projectRoot =
		App->fs->GetProjectRoot().lexically_normal();
	if (modelAssetIndex_.IsOpen() &&
		indexedProjectRoot_ == projectRoot)
	{
		return;
	}

	modelAssetError_.clear();
	if (modelAssetIndex_.OpenProject(projectRoot, modelAssetError_))
		indexedProjectRoot_ = projectRoot;
	else
		indexedProjectRoot_.clear();
}

void PanelGOTree::DrawModelPrefabMenu()
{
	const bool menuOpen = ImGui::BeginMenu("Model");
	if (!menuOpen)
	{
		modelMenuWasOpen_ = false;
		return;
	}

	EnsureModelAssetIndex();
	if (!modelMenuWasOpen_ && modelAssetIndex_.IsOpen())
	{
		modelAssetError_.clear();
		modelAssetIndex_.Refresh(modelAssetError_);
	}
	modelMenuWasOpen_ = true;

	const std::vector<const EGE::AssetEntry*> models =
		modelAssetIndex_.QueryAll(EGE::AssetKind::Model);
	if (models.empty())
	{
		ImGui::MenuItem(
			modelAssetError_.empty()
				? "No model assets in this project"
				: "Model assets are unavailable",
			nullptr,
			false,
			false);
		if (!modelAssetError_.empty() && ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", modelAssetError_.c_str());
	}

	for (const EGE::AssetEntry* asset : models)
	{
		if (!asset)
			continue;

		const Resource* imported =
			App->resources->FindResourceBySourceFile(
				Resource::model,
				asset->sourcePath);
		const std::string label =
			asset->relativePath.generic_string();

		ImGui::PushID(asset->sourcePath.c_str());
		if (ImGui::MenuItem(
				label.c_str(),
				imported ? nullptr : "Import"))
		{
			if (imported)
			{
				if (!App->level->AddModel(imported->GetUID()))
					LOG("Could not add model [%s] to the scene",
						asset->sourcePath.c_str());
			}
			else
			{
				modelImportDialog_.Open(asset->sourcePath);
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(
				imported
					? "Add this imported model to the scene."
					: "Import this model, then add it to the scene.");
		}
		ImGui::PopID();
	}

	ImGui::EndMenu();
}

void PanelGOTree::DrawPrefabAssetMenu()
{
	if (!ImGui::BeginMenu("Prefab", App->IsStop()))
		return;

	EnsureModelAssetIndex();
	modelAssetError_.clear();
	if (modelAssetIndex_.IsOpen())
		modelAssetIndex_.Refresh(modelAssetError_);

	const std::vector<const EGE::AssetEntry*> prefabs =
		modelAssetIndex_.QueryAll(EGE::AssetKind::Prefab);
	if (prefabs.empty())
	{
		ImGui::MenuItem(
			modelAssetError_.empty()
				? "No prefab assets in this project"
				: "Prefab assets are unavailable",
			nullptr,
			false,
			false);
		if (!modelAssetError_.empty() && ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", modelAssetError_.c_str());
	}

	for (const EGE::AssetEntry* asset : prefabs)
	{
		if (!asset)
			continue;

		ImGui::PushID(asset->sourcePath.c_str());
		if (ImGui::MenuItem(asset->relativePath.generic_string().c_str()))
		{
			std::string error;
			if (GameObject* instance = App->level->InstantiatePrefab(
					asset->sourcePath.c_str(), nullptr, &error))
			{
				App->editor->SetSelected(instance);
				App->editor->SetProjectStatus(
					true, asset->name + " instantiated.");
			}
			else
			{
				App->editor->SetProjectStatus(
					false,
					error.empty()
						? "The prefab could not be instantiated."
						: error);
			}
		}
		ImGui::PopID();
	}

	ImGui::EndMenu();
}

void PanelGOTree::DrawModelImportDialog()
{
	modelImportDialog_.Display();
	if (!modelImportDialog_.HasSelection())
		return;

	const std::string sourcePath = modelImportDialog_.GetFile();
	const UID uid = App->resources->ImportModel(
		sourcePath.c_str(),
		modelImportDialog_.GetOptions());
	modelImportDialog_.ClearSelection();

	if (uid == 0)
	{
		LOG("Could not import model [%s]", sourcePath.c_str());
		return;
	}

	if (!App->level->AddModel(uid))
	{
		LOG("Imported model [%s], but it could not be added to the scene",
			sourcePath.c_str());
		return;
	}

	if (App->editor->assets)
		App->editor->assets->RefreshProjectAssets();
}

// ---------------------------------------------------------
bool PanelGOTree::RecursiveDraw(GameObject* go)
{
    bool stop = false;
	const std::string label =
		(go->name.empty() ? "(empty)" : go->name) +
		"##GameObject_" + std::to_string(go->GetUID());
	uint flags =
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;

    const char* str = strstr(go->name.c_str(), "$AssimpFbx$");
    if (str != nullptr)
    {
        for (GameObject* go : go->childs) 
        {
            if((stop = RecursiveDraw(go)) == true)
            {
                break;
            }
        }
    }
    else
    {
        if (go->childs.size() == 0)
            flags |= ImGuiTreeNodeFlags_Leaf;


		GameObject* selected_go =
			App->editor->GetPrimaryGameObject();

        if (App->editor->IsGameObjectSelected(go))
        {
            flags |= ImGuiTreeNodeFlags_Selected;
            open_selected = false;
        }

        ImVec4 color = IMGUI_WHITE;

        if (go->IsActive() == false)
            color = IMGUI_RED;

        if (go->visible == false)
            color = IMGUI_GREY;

        if (go->WasBBoxDirty() == true)
            color = IMGUI_GREEN;

        if (go->WasDirty() == true)
            color = IMGUI_YELLOW;

        ImGui::PushStyleColor(ImGuiCol_Text, color);

        if (open_selected == true && selected_go && selected_go->IsUnder(go) == true)
            ImGui::SetNextTreeNodeOpen(true);

        if (ImGui::TreeNodeEx(label.c_str(), flags))
        {
            CheckHover(go);

            if (ImGui::BeginPopupContextItem())
            {
				if (ImGui::MenuItem("Create Child"))
				{
					GameObject* child =
						App->level->CreateGameObject(go);
					if (child)
						App->editor->SetSelected(child);
				}
				if (ImGui::MenuItem("Duplicate"))
					DuplicateSelection(go);
				if (ImGui::MenuItem("Rename", "F2"))
					RequestRenameSelection(go);
				if (ImGui::MenuItem("Frame Selected", "F"))
				{
					if (!App->editor->IsGameObjectSelected(go))
						App->editor->SetSelected(go);
					FrameSelection();
				}
				if (ImGui::MenuItem(
						"Create Prefab...",
						nullptr,
						false,
						App->IsStop() && App->editor->assets))
				{
					App->editor->assets->BeginCreatePrefab(go);
					ImGui::SetWindowFocus(
						App->editor->assets->GetName());
				}
				if (ImGui::MenuItem("Delete", "Delete"))
					RequestDeleteSelection(go);

                ImGui::EndPopup();
            }

            if(!stop)
            {
                for (list<GameObject*>::const_iterator it = go->childs.begin(); it != go->childs.end(); ++it)
                    if ((stop = RecursiveDraw(*it)) == true) break;
            }

            ImGui::TreePop();
        }
        else
            CheckHover(go);

        ImGui::PopStyleColor();
    }

    return stop;
}


void PanelGOTree::CheckHover(GameObject* go)
{
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly))
	{
		if (drag && drag != go)
		{
			ImGui::BeginTooltip();
			ImGui::Text("Move %s to %s", drag->name.c_str(), go->name.c_str());
			ImGui::EndTooltip();
		}

		if (ImGui::IsMouseClicked(0)) {
			HandleSelectionClick(go);
			drag_candidate = go;
		}

		if (ImGui::IsMouseDoubleClicked(0))
		{
			float radius = go->global_bbox.MinimalEnclosingSphere().r;
			App->camera->CenterOn(go->GetGlobalPosition(), std::fmaxf(radius, 5.0f) * 2.0f);
		}

		if (drag &&
			ImGui::IsMouseReleased(0) &&
			drag != go &&
			!go->IsUnder(drag))
		{
			drag->SetNewParent(go, true);
			drag = nullptr;
			drag_candidate = nullptr;
		}
	}
}

