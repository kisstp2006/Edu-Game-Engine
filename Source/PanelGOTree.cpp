#include "Globals.h"
#include "Application.h"
#include "PanelGOTree.h"
#include <imgui.h>
#include "Application.h"
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
	if (!drag &&
		drag_candidate &&
		ImGui::IsMouseDragging(ImGuiMouseButton_Left, 6.0f))
	{
		drag = drag_candidate;
	}
	//ImGui::SetNextWindowContentWidth((float) (width*2));
    //ImGui::Begin("GameObjects Hierarchy", &active, 
		//ImGuiWindowFlags_NoResize | 
		//ImGuiWindowFlags_NoFocusOnAppearing |
		//ImGuiWindowFlags_HorizontalScrollbar );

	if (ImGui::BeginMenu("Options"))
	{
		if (ImGui::MenuItem("Load..", nullptr, false, App->IsStop()))
			App->editor->RequestOpenScene();

		if (ImGui::MenuItem("Save..", nullptr, false, App->IsStop()))
			App->editor->RequestSaveScene(true);

		if (ImGui::BeginMenu("Load Prefab"))
		{
			DrawModelPrefabMenu();
            ImGui::EndMenu();
		}

		if(ImGui::MenuItem("New Game Object"))
			App->level->CreateGameObject();

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

	GameObject* root = App->level->GetRoot();
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


        GameObject* const* selected_go = std::get_if<GameObject*>(&App->editor->GetSelection());

        if (selected_go && go == *selected_go)
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

        if (open_selected == true && selected_go && (*selected_go)->IsUnder(go) == true)
            ImGui::SetNextTreeNodeOpen(true);

        if (ImGui::TreeNodeEx(label.c_str(), flags))
        {
            CheckHover(go);

			if (ImGui::IsItemClicked(0)) {
				App->editor->SetSelected(go);
				drag_candidate = go;
			}

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
                    App->level->Duplicate(go);
				if (ImGui::MenuItem("Remove"))
				{
					App->editor->ClearSelected();
					drag = nullptr;
					drag_candidate = nullptr;
					go->Remove();
					stop = true;
                }

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
            App->editor->SetSelected(go);
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

