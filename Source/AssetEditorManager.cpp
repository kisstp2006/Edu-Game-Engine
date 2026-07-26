#include "AssetEditorManager.h"

#include "Application.h"
#include "BatchManager.h"
#include "EditorAssetSelection.h"
#include "HashString.h"
#include "ModuleFileSystem.h"
#include "ModuleRenderer.h"
#include "ModuleResources.h"
#include "MaterialPreviewRenderer.h"
#include "ResourceAnimation.h"
#include "ResourceMaterial.h"
#include "ResourceMesh.h"
#include "ResourceStateMachine.h"
#include "StateViewport.h"

#include "imgui.h"
#include "imgui_node_editor.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace ed = ax::NodeEditor;

namespace
{
	const ImVec4 Accent(0.20f, 0.58f, 0.95f, 1.0f);
	const ImVec4 Success(0.35f, 0.82f, 0.48f, 1.0f);
	const ImVec4 Warning(0.95f, 0.68f, 0.25f, 1.0f);

	const char* EditorTypeName(Resource::Type type)
	{
		switch (type)
		{
		case Resource::material: return "Material Editor";
		case Resource::state_machine: return "State Machine Editor";
		case Resource::mesh: return "Procedural Mesh Editor";
		default: return "Asset Editor";
		}
	}

	bool IsEditableResource(Resource::Type type)
	{
		return type == Resource::material ||
			type == Resource::state_machine ||
			type == Resource::mesh;
	}

	bool DrawResourceReference(
		const char* label,
		Resource::Type type,
		UID& selectedUid)
	{
		std::vector<const Resource*> resources;
		App->resources->GatherResourceType(resources, type);
		std::sort(
			resources.begin(), resources.end(),
			[](const Resource* left, const Resource* right)
			{
				return std::strcmp(
					left->GetUserResName(),
					right->GetUserResName()) < 0;
			});

		const Resource* selected = App->resources->Get(selectedUid);
		const char* preview =
			selected ? selected->GetUserResName() : "None";
		bool changed = false;
		if (ImGui::BeginCombo(label, preview))
		{
			if (ImGui::Selectable("None", selectedUid == 0))
			{
				selectedUid = 0;
				changed = true;
			}
			for (const Resource* resource : resources)
			{
				const bool isSelected = resource->GetUID() == selectedUid;
				if (ImGui::Selectable(
						resource->GetUserResName(), isSelected))
				{
					selectedUid = resource->GetUID();
					changed = true;
				}
				if (isSelected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	void DrawTextureReference(const char* label, UID& uid, bool& changed)
	{
		ImGui::PushID(label);
		ImGui::TextDisabled("%s", label);
		changed =
			DrawResourceReference("##Texture", Resource::texture, uid) ||
			changed;
		ImGui::PopID();
	}

	void DrawEditorToolbar(
		const char* typeName,
		const std::string& sourcePath,
		const char* status)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, Accent);
		ImGui::TextUnformatted(typeName);
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		ImGui::TextDisabled("%s", sourcePath.c_str());
		if (status && status[0] != '\0')
		{
			ImGui::SameLine();
			const ImVec4 color =
				std::strncmp(status, "Error", 5) == 0 ? Warning : Success;
			ImGui::TextColored(color, "%s", status);
		}
		ImGui::Separator();
	}

	void DrawSectionTitle(const char* title)
	{
		ImGui::Spacing();
		ImGui::TextDisabled("%s", title);
		ImGui::Separator();
	}

	const char* MeshShapeName(ModuleResources::ProceduralMeshShape shape)
	{
		switch (shape)
		{
		case ModuleResources::ProceduralMeshShape::Plane: return "Plane";
		case ModuleResources::ProceduralMeshShape::Cube: return "Cube";
		case ModuleResources::ProceduralMeshShape::Sphere: return "Sphere";
		case ModuleResources::ProceduralMeshShape::Cylinder: return "Cylinder";
		case ModuleResources::ProceduralMeshShape::Cone: return "Cone";
		case ModuleResources::ProceduralMeshShape::Torus: return "Torus";
		}
		return "Unknown";
	}
}

namespace EGE
{
	struct AssetEditorManager::Impl
	{
		struct OpenEditor
		{
			EditorAssetSelection asset;
			Resource::Type type = Resource::unknown;
			std::string windowTitle;
			std::string nodeSettingsFile;
			std::string status;
			std::array<char, 128> name = {};
			bool open = true;
			bool requestFocus = true;
			bool ownsResourceReference = false;

			std::unique_ptr<StateViewport> stateViewport;
			ed::EditorContext* nodeContext = nullptr;
			std::unique_ptr<MaterialPreviewRenderer> materialPreview;

			ModuleResources::ProceduralMeshSettings meshSettings;
			bool meshSettingsLoaded = false;
		};

		std::vector<std::unique_ptr<OpenEditor>> editors;

		~Impl()
		{
			CloseAll();
		}

		bool Open(
			const EditorAssetSelection& asset,
			std::string& error)
		{
			if (asset.kind != AssetKind::Material &&
				asset.kind != AssetKind::StateMachine &&
				asset.kind != AssetKind::Mesh)
			{
				error =
					"This source asset does not have a dedicated editor.";
				return false;
			}
			if (asset.directory || asset.primaryResource == 0)
			{
				error = "This asset does not have an editable engine resource.";
				return false;
			}

			Resource* resource =
				App->resources->Get(asset.primaryResource);
			if (!resource || !IsEditableResource(resource->GetType()))
			{
				error =
					"Only materials, animation state machines and "
					"procedural meshes have dedicated editors.";
				return false;
			}

			const auto existing = std::find_if(
				editors.begin(), editors.end(),
				[&asset](const std::unique_ptr<OpenEditor>& editor)
				{
					return editor->asset.primaryResource ==
						asset.primaryResource;
				});
			if (existing != editors.end())
			{
				(*existing)->open = true;
				(*existing)->requestFocus = true;
				return true;
			}

			if (!resource->LoadToMemory())
			{
				error = "The asset resource could not be loaded.";
				return false;
			}

			auto editor = std::make_unique<OpenEditor>();
			editor->asset = asset;
			editor->type = resource->GetType();
			editor->ownsResourceReference = true;
			strncpy_s(
				editor->name.data(), editor->name.size(),
				resource->GetUserResName(), _TRUNCATE);
			editor->windowTitle =
				std::string(resource->GetUserResName()) + " - " +
				EditorTypeName(editor->type) + "###AssetEditor" +
				std::to_string(resource->GetUID());

			if (editor->type == Resource::state_machine)
			{
				const std::filesystem::path settingsDirectory =
					App->fs->GetProjectRoot() /
					"Settings" / "AssetEditors";
				std::error_code directoryError;
				std::filesystem::create_directories(
					settingsDirectory, directoryError);
				editor->nodeSettingsFile =
					(settingsDirectory /
						(std::to_string(resource->GetUID()) + ".json"))
					.string();

				ed::Config config;
				config.SettingsFile = editor->nodeSettingsFile.c_str();
				editor->nodeContext = ed::CreateEditor(&config);
				editor->stateViewport = std::make_unique<StateViewport>();
			}
			else if (editor->type == Resource::material)
			{
				editor->materialPreview =
					std::make_unique<MaterialPreviewRenderer>();
			}
			else if (editor->type == Resource::mesh)
			{
				std::string meshName;
				editor->meshSettingsLoaded =
					App->resources->LoadProceduralMeshSettings(
						asset.sourcePath.c_str(),
						editor->meshSettings,
						meshName,
						editor->status);
				if (editor->meshSettingsLoaded && !meshName.empty())
				{
					strncpy_s(
						editor->name.data(), editor->name.size(),
						meshName.c_str(), _TRUNCATE);
					editor->status.clear();
				}
			}

			editors.push_back(std::move(editor));
			return true;
		}

		void CloseEditor(OpenEditor& editor)
		{
			editor.stateViewport.reset();
			editor.materialPreview.reset();
			if (editor.nodeContext)
			{
				ed::DestroyEditor(editor.nodeContext);
				editor.nodeContext = nullptr;
			}
			if (editor.ownsResourceReference)
			{
				if (Resource* resource =
						App->resources->Get(editor.asset.primaryResource))
				{
					resource->Release();
				}
				editor.ownsResourceReference = false;
			}
		}

		void CloseAll()
		{
			for (const std::unique_ptr<OpenEditor>& editor : editors)
				CloseEditor(*editor);
			editors.clear();
		}

		void Draw()
		{
			for (const std::unique_ptr<OpenEditor>& editor : editors)
				DrawEditor(*editor);

			const auto firstClosed = std::remove_if(
				editors.begin(), editors.end(),
				[this](const std::unique_ptr<OpenEditor>& editor)
				{
					if (editor->open)
						return false;
					CloseEditor(*editor);
					return true;
				});
			editors.erase(firstClosed, editors.end());
		}

		void DrawEditor(OpenEditor& editor)
		{
			Resource* resource =
				App->resources->Get(editor.asset.primaryResource);
			if (!resource || resource->GetType() != editor.type)
			{
				editor.open = false;
				return;
			}

			ImGui::SetNextWindowSize(
				ImVec2(940.0f, 660.0f), ImGuiCond_FirstUseEver);
			if (editor.requestFocus)
			{
				ImGui::SetNextWindowFocus();
				editor.requestFocus = false;
			}
			if (ImGui::Begin(
					editor.windowTitle.c_str(),
					&editor.open,
					ImGuiWindowFlags_NoFocusOnAppearing))
			{
				DrawEditorToolbar(
					EditorTypeName(editor.type),
					editor.asset.sourcePath,
					editor.status.c_str());
				ImGui::PushID(static_cast<int>(
					editor.asset.primaryResource));
				switch (editor.type)
				{
				case Resource::material:
					DrawMaterialEditor(
						editor,
						*static_cast<ResourceMaterial*>(resource));
					break;
				case Resource::state_machine:
					DrawStateMachineEditor(
						editor,
						*static_cast<ResourceStateMachine*>(resource));
					break;
				case Resource::mesh:
					DrawMeshEditor(
						editor,
						*static_cast<ResourceMesh*>(resource));
					break;
				default:
					break;
				}
				ImGui::PopID();
			}
			ImGui::End();
		}

		void DrawMaterialEditor(
			OpenEditor& editor,
			ResourceMaterial& material)
		{
			bool modified = false;
			ImGui::BeginChild(
				"MaterialSummary", ImVec2(375.0f, 0.0f), true);
			ImGui::TextDisabled("MATERIAL");
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			if (editor.materialPreview)
			{
				editor.materialPreview->Draw(
					material,
					ImVec2(
						ImGui::GetContentRegionAvail().x,
						330.0f));
			}
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::TextWrapped("%s", material.GetUserResName());
			ImGui::TextDisabled(
				"%s",
				material.GetWorkFlow() == MetallicRoughness
					? "Metallic / Roughness"
					: "Specular / Glossiness");
			ImGui::TextDisabled(
				"UID %llu",
				static_cast<unsigned long long>(material.GetUID()));
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("MaterialProperties", ImVec2(0.0f, 0.0f));
			if (ImGui::InputText(
					"Name", editor.name.data(), editor.name.size()))
			{
				material.SetUserResName(editor.name.data());
				modified = true;
			}

			bool doubleSided = material.GetDoubleSided();
			if (ImGui::Checkbox("Double sided", &doubleSided))
			{
				material.SetDoubleSided(doubleSided);
				modified = true;
			}
			bool planar = material.GetPlanarReflections();
			if (ImGui::Checkbox("Planar reflections", &planar))
			{
				material.SetPlanarReflections(planar);
				modified = true;
			}
			float alphaTest = material.GetAlphaTest();
			if (ImGui::SliderFloat(
					"Alpha cutoff", &alphaTest, 0.0f, 1.0f))
			{
				material.SetAlphaTest(alphaTest);
				modified = true;
			}

			float2 tiling = material.GetUVTiling();
			if (ImGui::DragFloat2(
					"UV tiling", &tiling.x, 0.05f, 0.01f, 100.0f))
			{
				material.SetUVTiling(tiling);
				modified = true;
			}
			float2 offset = material.GetUVOffset();
			if (ImGui::DragFloat2(
					"UV offset", &offset.x, 0.01f, -10.0f, 10.0f))
			{
				material.SetUVOffset(offset);
				modified = true;
			}

			DrawSectionTitle("SURFACE");
			if (material.GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material.GetMetallicRoughData();
				modified =
					ImGui::ColorEdit4(
						"Base color", &data.baseColor.x) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Metalness", &data.metalness, 0.0f, 1.0f) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Roughness", &data.roughness, 0.0f, 1.0f) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Normal strength",
						&data.normal_strength, 0.0f, 10.0f) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Occlusion strength",
						&data.occlusion_strength, 0.0f, 10.0f) ||
					modified;
				modified =
					ImGui::ColorEdit3(
						"Emissive color", &data.emissive_color.x) ||
					modified;
				modified =
					ImGui::DragFloat(
						"Emissive intensity",
						&data.emissive_intensity,
						0.05f, 0.0f, 100.0f) ||
					modified;

				DrawSectionTitle("TEXTURES");
				DrawTextureReference(
					"Base color", data.textures[MR_TextureBaseColor],
					modified);
				DrawTextureReference(
					"Metallic / Roughness",
					data.textures[MR_TextureMetallicRough], modified);
				DrawTextureReference(
					"Normal", data.textures[MR_TextureNormal], modified);
				DrawTextureReference(
					"Occlusion",
					data.textures[MR_TextureOcclusion], modified);
				DrawTextureReference(
					"Emissive",
					data.textures[MR_TextureEmissive], modified);
				if (modified)
					material.SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material.GetSpecularGlossData();
				modified =
					ImGui::ColorEdit4(
						"Diffuse color", &data.diffuse_color.x) ||
					modified;
				modified =
					ImGui::ColorEdit3(
						"Specular color", &data.specular_color.x) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Smoothness", &data.smoothness, 0.0f, 1.0f) ||
					modified;
				modified =
					ImGui::DragFloat(
						"Specular intensity",
						&data.specular_intensity,
						0.05f, 0.0f, 100.0f) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Normal strength",
						&data.normal_strength, 0.0f, 10.0f) ||
					modified;
				modified =
					ImGui::SliderFloat(
						"Occlusion strength",
						&data.occlusion_strength, 0.0f, 10.0f) ||
					modified;
				modified =
					ImGui::ColorEdit3(
						"Emissive color", &data.emissive_color.x) ||
					modified;
				modified =
					ImGui::DragFloat(
						"Emissive intensity",
						&data.emissive_intensity,
						0.05f, 0.0f, 100.0f) ||
					modified;

				DrawSectionTitle("TEXTURES");
				DrawTextureReference(
					"Diffuse", data.textures[SG_TextureDiffuse], modified);
				DrawTextureReference(
					"Specular", data.textures[SG_TextureSpecular], modified);
				DrawTextureReference(
					"Normal", data.textures[SG_TextureNormal], modified);
				DrawTextureReference(
					"Occlusion", data.textures[SG_TextureOcclusion], modified);
				DrawTextureReference(
					"Emissive", data.textures[SG_TextureEmissive], modified);
				if (modified)
					material.SetSpecularGlossData(data);
			}

			if (modified)
			{
				if (material.Save())
				{
					editor.status = "Saved";
					App->resources->SaveResources();
					App->renderer->GetBatchManager()->
						OnMaterialModified(material.GetUID());
				}
				else
				{
					editor.status = "Error: material save failed";
				}
			}
			ImGui::EndChild();
		}

		void DrawStateMachineEditor(
			OpenEditor& editor,
			ResourceStateMachine& stateMachine)
		{
			ImGui::BeginChild(
				"StateMachineSidebar", ImVec2(285.0f, 0.0f), true);
			ImGui::TextDisabled("ANIMATION CLIPS");
			ImGui::SameLine();
			if (ImGui::SmallButton("+ Add"))
			{
				const std::string clipName =
					"Clip " +
					std::to_string(stateMachine.GetNumClips() + 1);
				stateMachine.AddClip(
					HashString(clipName.c_str()), 0, true);
				stateMachine.Save();
				editor.status = "Saved";
			}
			ImGui::Separator();

			int removeClip = -1;
			for (uint index = 0;
				index < stateMachine.GetNumClips(); ++index)
			{
				ImGui::PushID(static_cast<int>(index));
				const bool open = ImGui::TreeNodeEx(
					"Clip",
					ImGuiTreeNodeFlags_DefaultOpen,
					"%s", stateMachine.GetClipName(index).C_str());
				if (open)
				{
					bool modified = false;
					char clipName[128] = {};
					strncpy_s(
						clipName, sizeof(clipName),
						stateMachine.GetClipName(index).C_str(),
						_TRUNCATE);
					if (ImGui::InputText(
							"Name", clipName, sizeof(clipName)))
					{
						stateMachine.SetClipName(
							index, HashString(clipName));
						modified = true;
					}

					UID animationUid =
						stateMachine.GetClipRes(index);
					if (DrawResourceReference(
							"Animation",
							Resource::animation,
							animationUid))
					{
						stateMachine.SetClipRes(index, animationUid);
						modified = true;
					}

					bool loop = stateMachine.GetClipLoop(index);
					if (ImGui::Checkbox("Loop", &loop))
					{
						stateMachine.SetClipLoop(index, loop);
						modified = true;
					}
					if (ImGui::SmallButton("Remove clip"))
						removeClip = static_cast<int>(index);
					if (modified)
					{
						stateMachine.Save();
						editor.status = "Saved";
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			if (removeClip >= 0)
			{
				stateMachine.RemoveClip(
					static_cast<uint>(removeClip));
				stateMachine.Save();
				editor.status = "Saved";
			}
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild(
				"StateMachineGraph", ImVec2(0.0f, 0.0f), true);
			if (editor.stateViewport && editor.nodeContext)
				editor.stateViewport->Draw(
					&stateMachine, editor.nodeContext);
			ImGui::EndChild();
		}

		void DrawMeshEditor(
			OpenEditor& editor,
			ResourceMesh& mesh)
		{
			ImGui::BeginChild(
				"MeshSummary", ImVec2(245.0f, 0.0f), true);
			ImGui::TextDisabled("PROCEDURAL MESH");
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::TextWrapped("%s", mesh.GetUserResName());
			ImGui::TextDisabled(
				"%s",
				editor.meshSettingsLoaded
					? MeshShapeName(editor.meshSettings.shape)
					: "Read only mesh");
			ImGui::Separator();
			ImGui::Text("Vertices  %u", mesh.GetNumVertices());
			ImGui::Text("Triangles %u", mesh.GetNumIndices() / 3);
			ImGui::TextDisabled(
				"UID %llu",
				static_cast<unsigned long long>(mesh.GetUID()));
			ImGui::EndChild();

			ImGui::SameLine();
			ImGui::BeginChild("MeshProperties", ImVec2(0.0f, 0.0f));
			if (!editor.meshSettingsLoaded)
			{
				ImGui::TextWrapped(
					"This mesh was not created from an editable "
					"procedural mesh descriptor.");
				ImGui::EndChild();
				return;
			}

			ImGui::InputText(
				"Name", editor.name.data(), editor.name.size());

			static const char* shapes[] = {
				"Plane", "Cube", "Sphere",
				"Cylinder", "Cone", "Torus"};
			int shapeIndex =
				static_cast<int>(editor.meshSettings.shape);
			if (ImGui::Combo(
					"Shape", &shapeIndex,
					shapes, static_cast<int>(std::size(shapes))))
			{
				editor.meshSettings.shape =
					static_cast<
						ModuleResources::ProceduralMeshShape>(
							shapeIndex);
			}

			const ModuleResources::ProceduralMeshShape shape =
				editor.meshSettings.shape;
			if (shape == ModuleResources::ProceduralMeshShape::Plane)
			{
				ImGui::DragFloat(
					"Width", &editor.meshSettings.width,
					0.05f, 0.01f, 1000.0f);
				ImGui::DragFloat(
					"Height", &editor.meshSettings.height,
					0.05f, 0.01f, 1000.0f);
			}
			else if (shape == ModuleResources::ProceduralMeshShape::Cube)
			{
				ImGui::DragFloat(
					"Size", &editor.meshSettings.width,
					0.05f, 0.01f, 1000.0f);
			}
			else if (shape == ModuleResources::ProceduralMeshShape::Sphere)
			{
				ImGui::DragFloat(
					"Radius", &editor.meshSettings.radius,
					0.05f, 0.01f, 1000.0f);
			}
			else if (
				shape == ModuleResources::ProceduralMeshShape::Cylinder ||
				shape == ModuleResources::ProceduralMeshShape::Cone)
			{
				ImGui::DragFloat(
					"Height", &editor.meshSettings.height,
					0.05f, 0.01f, 1000.0f);
				ImGui::DragFloat(
					"Radius", &editor.meshSettings.radius,
					0.05f, 0.01f, 1000.0f);
			}
			else if (shape == ModuleResources::ProceduralMeshShape::Torus)
			{
				ImGui::DragFloat(
					"Inner radius",
					&editor.meshSettings.innerRadius,
					0.02f, 0.01f, 1.0f);
				ImGui::DragFloat(
					"Outer radius",
					&editor.meshSettings.outerRadius,
					0.05f, 0.01f, 1000.0f);
			}

			if (shape != ModuleResources::ProceduralMeshShape::Cube)
			{
				int slices =
					static_cast<int>(editor.meshSettings.slices);
				int stacks =
					static_cast<int>(editor.meshSettings.stacks);
				ImGui::DragInt("Slices", &slices, 1.0f, 3, 256);
				ImGui::DragInt("Stacks", &stacks, 1.0f, 1, 256);
				editor.meshSettings.slices =
					static_cast<unsigned>(std::max(slices, 3));
				editor.meshSettings.stacks =
					static_cast<unsigned>(std::max(stacks, 1));
			}

			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			ImGui::PushStyleColor(ImGuiCol_Button, Accent);
			if (ImGui::Button("Apply and regenerate", ImVec2(190.0f, 34.0f)))
			{
				std::string error;
				if (App->resources->UpdateProceduralMeshAsset(
						mesh.GetUID(),
						editor.asset.sourcePath.c_str(),
						editor.name.data(),
						editor.meshSettings,
						error))
				{
					editor.status = "Saved";
				}
				else
				{
					editor.status = "Error: " + error;
				}
			}
			ImGui::PopStyleColor();
			ImGui::SameLine();
			ImGui::TextDisabled(
				"Regenerates in place; references keep the same UID.");
			ImGui::EndChild();
		}
	};

	AssetEditorManager::AssetEditorManager()
		: impl_(std::make_unique<Impl>())
	{
	}

	AssetEditorManager::~AssetEditorManager() = default;

	bool AssetEditorManager::Open(
		const EditorAssetSelection& asset,
		std::string& error)
	{
		return impl_->Open(asset, error);
	}

	void AssetEditorManager::Draw()
	{
		impl_->Draw();
	}

	void AssetEditorManager::CloseAll()
	{
		impl_->CloseAll();
	}
}
