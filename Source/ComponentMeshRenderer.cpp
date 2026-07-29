#include "Globals.h"

#include "ComponentMeshRenderer.h"
#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ModuleRenderer.h"
#include "ModuleLevelManager.h"
#include "BatchManager.h"

#include "OpenGL.h"
#include "Application.h"
#include "GameObject.h"

#include "ModuleResources.h"
#include "ModulePrograms.h"

#include "ResourceTexture.h"

#include "Leaks.h"

#include <algorithm>

ComponentMeshRenderer::ComponentMeshRenderer(GameObject* go) : Component(go, Types::MeshRenderer)
{
}

ComponentMeshRenderer::~ComponentMeshRenderer()
{
    if (batch_index != UINT_MAX)
    {
        if (App && App->renderer &&
            App->renderer->GetBatchManager())
        {
            App->renderer->GetBatchManager()->Remove(this);
        }
        batch_index = UINT_MAX;
    }

	Resource* res =
        App && App->resources
            ? App->resources->Get(mesh_resource)
            : nullptr;
	if (res != nullptr)
	{
		res->Release();
	}

    for (UID material : material_resources)
    {
        res =
            App && App->resources
                ? App->resources->Get(material)
                : nullptr;
        if (res != nullptr)
            res->Release();
    }

    delete [] node_cache;
    node_cache = nullptr;
}

void ComponentMeshRenderer::OnSave(Config& config) const 
{
	config.AddUID("MeshResource", mesh_resource);
	config.AddBool("Visible", visible);
	config.AddUInt(
		"Root", rootGO ? rootGO->GetUID() : root_go_uid);

	config.AddUID("MaterialResource", GetMaterialUID());
    config.AddArrayUID(
        "MaterialResources",
        material_resources.data(),
        static_cast<int>(material_resources.size()));
	config.AddBool("DebugDrawTangent", debug_draw_tangent);
	config.AddBool("CastShadows", cast_shadows);
	config.AddBool("RecvShadows", recv_shadows);
    config.AddUInt("RenderMode", render_mode);
    config.AddString("BatchName", batch_name.C_str());
    config.AddArray("SkinInfo");

    for(uint i=0; i< numBones; ++i)
    {
        const Bone& bone = bones[i];
        Config boneCfg;
        boneCfg.AddUID(
			"go", bone.go ? bone.go->GetUID() : bone.go_uid);
        boneCfg.AddFloat4x4("bind", bone.bind);
        config.AddArrayEntry(boneCfg);
    }
}

void ComponentMeshRenderer::OnLoad(Config* config) 
{
    ModuleLevelManager* level = App->level;

    visible            = config->GetBool("Visible", true);
    root_go_uid        = config->GetUInt("Root");
    rootGO             = level->Find(root_go_uid);

    debug_draw_tangent = config->GetBool("DebugDrawTangent", false);
    cast_shadows       = config->GetBool("CastShadows", true);
    recv_shadows       = config->GetBool("RecvShadows", true);
    render_mode        = config->GetUInt("RenderMode", RENDER_OPAQUE) == uint(RENDER_OPAQUE) ? RENDER_OPAQUE : RENDER_TRANSPARENT;

    ClearMaterialResources();
    const int materialCount =
        config->GetArrayCount("MaterialResources");
    if (materialCount > 0)
    {
        SetMaterialCount(
            static_cast<std::size_t>(materialCount));
        for (int index = 0; index < materialCount; ++index)
        {
            SetMaterialRes(
                static_cast<std::size_t>(index),
                config->GetUID("MaterialResources", 0, index));
        }
    }
    else
    {
        SetMaterialRes(
            config->GetUID("MaterialResource", 0));
    }
	SetMeshRes(config->GetUID("MeshResource", 0));

    if (GetMeshUID() != 0 && GetMaterialRes() != nullptr)
    {
        HashString batchName(config->GetString("BatchName"));
        SetBatchName(batchName ? batchName : HashString("default"));
    }

    numBones = config->GetArrayCount("SkinInfo");

    bones = std::make_unique<Bone[]>(numBones);

    for(uint i=0; i< numBones; ++i)
    {
        Bone& bone = bones[i];

        Config boneCfg = config->GetArray("SkinInfo", i);

        bone.go_uid = boneCfg.GetUInt("go");
        bone.go   = level->Find(bone.go_uid);
        bone.bind = boneCfg.GetFloat4x4("bind");
    }
}

void ComponentMeshRenderer::RemapSerializedReferences(
	const std::map<uint, uint>& gameObjectIds,
	const std::map<uint, uint>&)
{
	const auto remap = [&gameObjectIds](uint id)
	{
		if (id == 0)
			return uint{0};
		const auto found = gameObjectIds.find(id);
		return found == gameObjectIds.end() ? uint{0} : found->second;
	};

	root_go_uid = remap(root_go_uid);
	rootGO = App->level->Find(root_go_uid);
	for (uint index = 0; index < numBones; ++index)
	{
		bones[index].go_uid = remap(bones[index].go_uid);
		bones[index].go = App->level->Find(bones[index].go_uid);
	}
}

void ComponentMeshRenderer::GetBoundingBox (AABB& box) const 
{
    ResourceMesh* res = static_cast<ResourceMesh*>(App->resources->Get(mesh_resource));

    if(res != nullptr)
    {
        box.Enclose(res->bbox);
    }
}

void ComponentMeshRenderer::SetBatchName(const HashString& name)
{
    if(batch_index != UINT_MAX)
    {
        if (App && App->renderer &&
            App->renderer->GetBatchManager())
        {
            App->renderer->GetBatchManager()->Remove(this);
        }
        batch_index = UINT_MAX;
    }

    batch_name = name;

    if(batch_name &&
        GetMeshRes() &&
        GetMaterialRes() &&
        App &&
        App->renderer &&
        App->renderer->GetBatchManager())
    {
        batch_index = App->renderer->GetBatchManager()->Add(this, batch_name);
    }
}

bool ComponentMeshRenderer::SetMeshRes(UID uid) 
{
    if (uid == mesh_resource)
    {
        if (uid != 0 && batch_index == UINT_MAX)
        {
            SetBatchName(
                batch_name
                    ? batch_name
                    : HashString("default"));
        }
        return true;
    }

    ResourceMesh* newMesh = nullptr;
    if (uid != 0)
    {
        Resource* candidate =
            App && App->resources
                ? App->resources->Get(uid)
                : nullptr;
        if (!candidate || candidate->GetType() != Resource::mesh)
            return false;
        newMesh = static_cast<ResourceMesh*>(candidate);
        if (!newMesh->LoadToMemory())
            return false;
    }

    const HashString previousBatch = batch_name;
    SetBatchName(HashString());

    delete [] node_cache;
    node_cache = nullptr;

    morph_weights.reset();

    Resource* res =
        App && App->resources
            ? App->resources->Get(mesh_resource)
            : nullptr;

	if(res != nullptr)
	{
        assert(res->GetType() == Resource::mesh);
        res->Release();
	}

    mesh_resource = uid;
	if (newMesh != nullptr)
	{
            if(newMesh->GetNumMorphTargets())
            {
                morph_weights =
                    std::make_unique<float[]>(
                        newMesh->GetNumMorphTargets());
                std::fill_n(
                    morph_weights.get(),
                    newMesh->GetNumMorphTargets(),
                    0.0f);
            }
	}

    InvalidateBoundingBox();
    if (uid != 0)
    {
        SetBatchName(
            previousBatch
                ? previousBatch
                : HashString("default"));
    }
    else if (previousBatch)
    {
        SetBatchName(previousBatch);
    }
    return true;
}

const ResourceMesh* ComponentMeshRenderer::GetMeshRes() const
{
    const Resource* resource =
        App && App->resources
            ? App->resources->Get(mesh_resource)
            : nullptr;
	return resource && resource->GetType() == Resource::mesh
        ? static_cast<const ResourceMesh*>(resource)
        : nullptr;
}

ResourceMesh* ComponentMeshRenderer::GetMeshRes() 
{
    Resource* resource =
        App && App->resources
            ? App->resources->Get(mesh_resource)
            : nullptr;
	return resource && resource->GetType() == Resource::mesh
        ? static_cast<ResourceMesh*>(resource)
        : nullptr;
}

bool ComponentMeshRenderer::SetMaterialRes(UID uid)
{
    if (material_resources.empty())
        material_resources.push_back(0);
    return SetMaterialRes(0, uid);
}

bool ComponentMeshRenderer::SetMaterialRes(
    std::size_t index,
    UID uid)
{
    if (index >= material_resources.size())
        return false;
    if (material_resources[index] == uid)
        return true;

    ResourceMaterial* newMaterial = nullptr;
    if (uid != 0)
    {
        Resource* candidate =
            App && App->resources
                ? App->resources->Get(uid)
                : nullptr;
        if (!candidate ||
            candidate->GetType() != Resource::material)
        {
            return false;
        }
        newMaterial = static_cast<ResourceMaterial*>(candidate);
        if (!newMaterial->LoadToMemory())
            return false;
    }

    const bool primaryMaterial = index == 0;
    const HashString previousBatch = batch_name;
    if (primaryMaterial)
        SetBatchName(HashString());

    Resource* previous =
        App && App->resources
            ? App->resources->Get(material_resources[index])
            : nullptr;
    if (previous)
    {
        assert(previous->GetType() == Resource::material);
        previous->Release();
    }
    material_resources[index] = uid;

    if (primaryMaterial && previousBatch)
        SetBatchName(previousBatch);
    return true;
}

bool ComponentMeshRenderer::AddMaterialRes(UID uid)
{
    material_resources.push_back(0);
    if (uid == 0 ||
        SetMaterialRes(material_resources.size() - 1, uid))
    {
        return true;
    }
    material_resources.pop_back();
    return false;
}

bool ComponentMeshRenderer::RemoveMaterialRes(std::size_t index)
{
    if (index >= material_resources.size())
        return false;

    const bool primaryMaterial = index == 0;
    const HashString previousBatch = batch_name;
    if (primaryMaterial)
        SetBatchName(HashString());

    Resource* resource =
        App && App->resources
            ? App->resources->Get(material_resources[index])
            : nullptr;
    if (resource)
        resource->Release();
    material_resources.erase(material_resources.begin() + index);
    if (material_resources.empty())
        material_resources.push_back(0);

    if (primaryMaterial && previousBatch)
        SetBatchName(previousBatch);
    return true;
}

void ComponentMeshRenderer::ClearMaterialResources()
{
    const HashString previousBatch = batch_name;
    SetBatchName(HashString());
    if (App && App->resources)
    {
        for (UID material : material_resources)
        {
            if (Resource* resource = App->resources->Get(material))
                resource->Release();
        }
    }
    material_resources.assign(1, 0);
    if (previousBatch)
        SetBatchName(previousBatch);
}

void ComponentMeshRenderer::SetMaterialCount(std::size_t count)
{
    count = std::max<std::size_t>(count, 1);
    while (material_resources.size() > count)
        RemoveMaterialRes(material_resources.size() - 1);
    material_resources.resize(count, 0);
}

bool ComponentMeshRenderer::SetSkinInfo(const ResourceModel::Skin& skin, GameObject** gos)
{
    rootGO = skin.rootNode >= 0 ? gos[skin.rootNode] : nullptr;
	root_go_uid = rootGO ? rootGO->GetUID() : 0;
    numBones = uint32_t(skin.bones.size());
    bones = std::make_unique<Bone[]>(numBones);

    uint32_t index = 0;
    for (const ResourceModel::SkinBone& srcBone : skin.bones)
    {
        Bone& bone = bones[index++];
        bone.go = gos[srcBone.nodeIdx];
		bone.go_uid = bone.go ? bone.go->GetUID() : 0;
        bone.bind = srcBone.bind;
    }

    return true;
}

const ResourceMaterial* ComponentMeshRenderer::GetMaterialRes () const
{
    return GetMaterialRes(0);
}


ResourceMaterial* ComponentMeshRenderer::GetMaterialRes () 
{
    return GetMaterialRes(0);
}

const ResourceMaterial* ComponentMeshRenderer::GetMaterialRes(
    std::size_t index) const
{
    if (index >= material_resources.size())
        return nullptr;
    if (index == 0 && material_resources[index] == 0)
    {
        return App && App->resources
            ? App->resources->GetDefaultMaterial()
            : nullptr;
    }
    const Resource* resource =
        App && App->resources
            ? App->resources->Get(material_resources[index])
            : nullptr;
    return resource && resource->GetType() == Resource::material
        ? static_cast<const ResourceMaterial*>(resource)
        : nullptr;
}

ResourceMaterial* ComponentMeshRenderer::GetMaterialRes(
    std::size_t index)
{
    return const_cast<ResourceMaterial*>(
        static_cast<const ComponentMeshRenderer*>(this)
            ->GetMaterialRes(index));
}

UID ComponentMeshRenderer::GetMaterialUID() const
{
    return GetMaterialUID(0);
}

UID ComponentMeshRenderer::GetMaterialUID(
    std::size_t index) const
{
    return index < material_resources.size()
        ? material_resources[index]
        : 0;
}

std::size_t ComponentMeshRenderer::GetMaterialCount() const
{
    return material_resources.size();
}

const std::vector<UID>&
ComponentMeshRenderer::GetMaterialUIDs() const
{
    return material_resources;
}


void ComponentMeshRenderer::UpdateSkinPalette(float4x4* palette) const
{
    ResourceMesh* mesh = static_cast<ResourceMesh*>(App->resources->Get(mesh_resource));

	if(numBones > 0)
	{
        float4x4 rootT = float4x4::identity;

        for(unsigned i=0; i < numBones; ++i)
        {
            const Bone& bone = bones[i];
            const GameObject* bone_node = bone.go;

            float4x4 transform = bone_node->GetGlobalTransformation();
            palette[i] = bone_node ? rootT * transform * bone.bind :  float4x4::identity;
        }
    }

}

void ComponentMeshRenderer::UpdateCPUMorphTargets() const
{
    const ResourceMesh* mesh = GetMeshRes();

    if(dirty_morphs)
    {
        glBindBuffer(GL_ARRAY_BUFFER, mesh->GetVBO());

        float3* vertices = reinterpret_cast<float3*>(glMapBufferRange(GL_ARRAY_BUFFER, 0, mesh->GetNumVertices() * sizeof(float3), GL_MAP_WRITE_BIT));       
        memcpy(vertices, mesh->GetVertices(), sizeof(float3)*mesh->GetNumVertices());

        for(uint i=0; i< mesh->GetNumMorphTargets(); ++i)
        {
            const ResourceMesh::MorphData& morph_target = mesh->GetMorphTarget(i);

            if (morph_weights[i] > 0.0f)
            {
                for(uint j=0; j< mesh->num_indices; ++j)
                {
                    uint index = morph_target.src_indices[j];
                    vertices[index] += morph_target.src_vertices[index] * morph_weights[i];
                }
            }
        }

        glUnmapBuffer(GL_ARRAY_BUFFER);

        if(mesh->HasAttrib(ATTRIB_NORMALS))
        {
            float3* normals = reinterpret_cast<float3*>(glMapBufferRange(GL_ARRAY_BUFFER, mesh->GetOffset(ATTRIB_NORMALS), mesh->GetNumVertices() * sizeof(float3), GL_MAP_WRITE_BIT));
            memcpy(normals, mesh->GetNormals(), sizeof(float3)*mesh->GetNumVertices());

            for(uint i=0; i< mesh->GetNumMorphTargets(); ++i)
            {
                const ResourceMesh::MorphData& morph_target = mesh->GetMorphTarget(i);

                // Loas indices????
                if (morph_weights[i] > 0.0f)
                {
                    for(uint j=0; j< mesh->GetNumIndices(); ++j)
                    {
                        uint index = morph_target.src_indices[j];
                        normals[index] += morph_target.src_normals[index] * morph_weights[i];
                        normals[index].Normalize();
                    }
                }
            }

            glUnmapBuffer(GL_ARRAY_BUFFER);

            if(mesh->HasAttrib(ATTRIB_TANGENTS))
            {
                float3* tangents = reinterpret_cast<float3*>(glMapBufferRange(GL_ARRAY_BUFFER, mesh->GetOffset(ATTRIB_TANGENTS), mesh->GetNumVertices() * sizeof(float3), GL_MAP_WRITE_BIT)); 
                memcpy(tangents, mesh->GetTangents(), sizeof(float3)*mesh->GetNumVertices());

                for(uint i=0; i< mesh->GetNumMorphTargets(); ++i)
                {
                    const ResourceMesh::MorphData& morph_target = mesh->GetMorphTarget(i);
                    /* TODO
                    if (morph_weights[i] > 0.0f)
                    {
                        for(uint j=0; j< morph_target.num_indices; ++j)
                        {
                            uint index = morph_target.src_indices[j];
                            tangents[index] += morph_target.src_tangents[index] * morph_weights[i];
                            tangents[index].Normalize();
                        }
                    }
                    */
                }

                glUnmapBuffer(GL_ARRAY_BUFFER);
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        dirty_morphs = false;
    }
}
