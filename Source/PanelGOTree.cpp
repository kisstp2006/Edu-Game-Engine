#include "Globals.h"
#include "Application.h"
#include "PanelGOTree.h"
#include <imgui.h>
#include "Application.h"
#include "ModuleLevelManager.h"
#include "LightManager.h"
#include "ModuleEditor.h"
#include "ModuleEditorCamera.h"
#include "ModuleResources.h"
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
	node = 0;
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
			if (ImGui::BeginMenu("Model"))
			{
				vector<const Resource*> resources;
				App->resources->GatherResourceType(resources, Resource::model);

				for (vector<const Resource*>::const_iterator it = resources.begin(); it != resources.end(); ++it)
				{
					const Resource* model = (*it);
                    ImGui::PushID(model->GetExportedFile());
					if (ImGui::MenuItem(model->GetUserResName()))
					{
                        App->level->AddModel(model->GetUID());
					}
                    ImGui::PopID();
				}

				ImGui::EndMenu();
			}
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

	if (drag && ImGui::IsMouseReleased(0))
		drag = nullptr;

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

// ---------------------------------------------------------
bool PanelGOTree::RecursiveDraw(GameObject* go)
{
    bool stop = false;
	sprintf_s(name, 80, "%s##node_%i", go->name.empty() ? "(empty)": go->name.c_str(), node++);
	uint flags = 0;// ImGuiTreeNodeFlags_OpenOnArrow;

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

        if (ImGui::TreeNodeEx(name, flags))
        {
            CheckHover(go);

            if (ImGui::IsItemClicked(0)) {
                App->editor->SetSelected(go);
                drag = go;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Duplicate"))
                    App->level->Duplicate(go);
                if (ImGui::MenuItem("Remove"))
                {
                    App->editor->ClearSelected();
                    drag = nullptr;
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
			drag = go;
		}

		if (ImGui::IsMouseDoubleClicked(0))
		{
			float radius = go->global_bbox.MinimalEnclosingSphere().r;
			App->camera->CenterOn(go->GetGlobalPosition(), std::fmaxf(radius, 5.0f) * 2.0f);
		}

		if (drag && ImGui::IsMouseReleased(0) && drag != go)
		{
			drag->SetNewParent(go, true);
			drag = nullptr;
		}
	}
}

