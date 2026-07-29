#include "ScriptAssetBindings.h"

#include "ScriptMath.h"
#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../ModuleResources.h"
#include "../ResourceAnimation.h"
#include "../ResourceAudio.h"
#include "../ResourceMaterial.h"
#include "../ResourceMesh.h"
#include "../ResourceModel.h"
#include "../ResourceStateMachine.h"
#include "../ResourceTexture.h"

#include <angelscript.h>

#include <algorithm>
#include <array>
#include <string>

namespace EGE
{
	namespace
	{
		void SetScriptException(const char* message)
		{
			if (asIScriptContext* context = asGetActiveContext())
				context->SetException(message);
		}

		Resource* ResolveResource(
			const ScriptResourceReference* reference,
			Resource::Type expectedType,
			const char* typeName)
		{
			Resource* resource =
				reference ? reference->Resolve() : nullptr;
			if (!resource || resource->GetType() != expectedType)
			{
				const std::string message =
					std::string("The ") + typeName +
					" asset reference is no longer valid.";
				SetScriptException(message.c_str());
				return nullptr;
			}
			return resource;
		}

		template<typename ResourceType>
		ResourceType* ResolveTypedResource(
			const ScriptResourceReference* reference,
			Resource::Type expectedType,
			const char* typeName)
		{
			return static_cast<ResourceType*>(
				ResolveResource(reference, expectedType, typeName));
		}

		template<typename ResourceType>
		ResourceType* ResolveLoadedResource(
			const ScriptResourceReference* reference,
			Resource::Type expectedType,
			const char* typeName)
		{
			ResourceType* resource =
				ResolveTypedResource<ResourceType>(
					reference, expectedType, typeName);
			if (!resource)
				return nullptr;
			if (!resource->IsLoadedToMemory() &&
				!resource->LoadToMemory())
			{
				const std::string message =
					std::string("The ") + typeName +
					" asset could not be loaded.";
				SetScriptException(message.c_str());
				return nullptr;
			}
			return resource;
		}

		bool ResourcesEqual(
			const ScriptResourceReference* other,
			const ScriptResourceReference* reference)
		{
			return other == reference ||
				(other && reference &&
					other->GetResourceId() ==
						reference->GetResourceId() &&
					other->GetResourceType() ==
						reference->GetResourceType());
		}

		std::string GetResourceName(
			const ScriptResourceReference* reference)
		{
			return reference ? reference->GetName() : std::string();
		}

		void SetResourceName(
			const std::string& name,
			ScriptResourceReference* reference)
		{
			Resource* resource =
				reference ? reference->Resolve() : nullptr;
			if (!resource)
			{
				SetScriptException(
					"The asset reference is no longer valid.");
				return;
			}
			resource->SetName(name.c_str());
		}

		std::string GetResourcePath(
			const ScriptResourceReference* reference)
		{
			return reference ? reference->GetPath() : std::string();
		}

		std::string GetResourceTypeName(
			const ScriptResourceReference* reference)
		{
			Resource* resource =
				reference ? reference->Resolve() : nullptr;
			return resource
				? resource->GetTypeStr()
				: std::string();
		}

		bool GetResourceLoaded(
			const ScriptResourceReference* reference)
		{
			Resource* resource =
				reference ? reference->Resolve() : nullptr;
			return resource && resource->IsLoadedToMemory();
		}

		std::uint32_t GetResourceReferenceCount(
			const ScriptResourceReference* reference)
		{
			Resource* resource =
				reference ? reference->Resolve() : nullptr;
			return resource
				? resource->CountReferences()
				: 0;
		}

		bool SaveResource(
			ScriptResourceReference* reference)
		{
			Resource* resource =
				reference ? reference->Resolve() : nullptr;
			if (!resource || !App || !App->resources)
			{
				SetScriptException(
					"The asset cannot be saved because it is no longer valid.");
				return false;
			}

			bool saved = false;
			switch (resource->GetType())
			{
				case Resource::material:
					saved =
						static_cast<ResourceMaterial*>(resource)
							->Save();
					break;
				case Resource::texture:
					saved =
						static_cast<ResourceTexture*>(resource)
							->Save();
					break;
				case Resource::mesh:
					saved =
						static_cast<ResourceMesh*>(resource)
							->Save();
					break;
				case Resource::model:
					saved =
						static_cast<ResourceModel*>(resource)
							->Save();
					break;
				case Resource::animation:
					saved =
						static_cast<ResourceAnimation*>(resource)
							->Save();
					break;
				case Resource::state_machine:
					saved =
						static_cast<ResourceStateMachine*>(resource)
							->Save();
					break;
				case Resource::audio:
					saved = true;
					break;
				default:
					break;
			}
			if (saved)
				App->resources->SaveResources();
			return saved;
		}

		ResourceMaterial* ResolveMaterial(
			const ScriptResourceReference* reference)
		{
			return ResolveTypedResource<ResourceMaterial>(
				reference, Resource::material, "Material");
		}

		ScriptColor ToScriptColor(const float4& color)
		{
			return {color.x, color.y, color.z, color.w};
		}

		ScriptColor ToScriptColor(const float3& color)
		{
			return {color.x, color.y, color.z, 1.0f};
		}

		float4 ToFloat4(const ScriptColor& color)
		{
			return {color.r, color.g, color.b, color.a};
		}

		float3 ToFloat3(const ScriptColor& color)
		{
			return {color.r, color.g, color.b};
		}

		int GetMaterialWorkflow(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material
				? static_cast<int>(material->GetWorkFlow())
				: static_cast<int>(MetallicRoughness);
		}

		bool GetMaterialDoubleSided(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material && material->GetDoubleSided();
		}

		void SetMaterialDoubleSided(
			bool value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
				material->SetDoubleSided(value);
		}

		bool GetMaterialPlanarReflections(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material && material->GetPlanarReflections();
		}

		void SetMaterialPlanarReflections(
			bool value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
				material->SetPlanarReflections(value);
		}

		float GetMaterialAlphaCutoff(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material ? material->GetAlphaTest() : 0.0f;
		}

		void SetMaterialAlphaCutoff(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
				material->SetAlphaTest(std::clamp(value, 0.0f, 1.0f));
		}

		ScriptColor GetMaterialBaseColor(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return {};
			return material->GetWorkFlow() == MetallicRoughness
				? ToScriptColor(
					material->GetMetallicRoughData().baseColor)
				: ToScriptColor(
					material->GetSpecularGlossData().diffuse_color);
		}

		void SetMaterialBaseColor(
			const ScriptColor& value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return;
			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.baseColor = ToFloat4(value);
				material->SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.diffuse_color = ToFloat4(value);
				material->SetSpecularGlossData(data);
			}
		}

		float GetMaterialMetallic(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material &&
				material->GetWorkFlow() == MetallicRoughness
					? material->GetMetallicRoughData().metalness
					: 0.0f;
		}

		void SetMaterialMetallic(
			float value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material ||
				material->GetWorkFlow() != MetallicRoughness)
			{
				return;
			}
			MetallicRoughData data =
				material->GetMetallicRoughData();
			data.metalness = std::clamp(value, 0.0f, 1.0f);
			material->SetMetallicRoughData(data);
		}

		float GetMaterialRoughness(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return 0.0f;
			return material->GetWorkFlow() == MetallicRoughness
				? material->GetMetallicRoughData().roughness
				: 1.0f -
					material->GetSpecularGlossData().smoothness;
		}

		void SetMaterialRoughness(
			float value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return;
			value = std::clamp(value, 0.0f, 1.0f);
			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.roughness = value;
				material->SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.smoothness = 1.0f - value;
				material->SetSpecularGlossData(data);
			}
		}

		ScriptColor GetMaterialSpecularColor(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material &&
				material->GetWorkFlow() == SpecularGlossiness
					? ToScriptColor(
						material->GetSpecularGlossData()
							.specular_color)
					: ScriptColor{};
		}

		void SetMaterialSpecularColor(
			const ScriptColor& value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material ||
				material->GetWorkFlow() != SpecularGlossiness)
			{
				return;
			}
			SpecularGlossData data =
				material->GetSpecularGlossData();
			data.specular_color = ToFloat3(value);
			material->SetSpecularGlossData(data);
		}

		ScriptColor GetMaterialEmissiveColor(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return {};
			return material->GetWorkFlow() == MetallicRoughness
				? ToScriptColor(
					material->GetMetallicRoughData()
						.emissive_color)
				: ToScriptColor(
					material->GetSpecularGlossData()
						.emissive_color);
		}

		void SetMaterialEmissiveColor(
			const ScriptColor& value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return;
			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.emissive_color = ToFloat3(value);
				material->SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.emissive_color = ToFloat3(value);
				material->SetSpecularGlossData(data);
			}
		}

		float GetMaterialEmissiveIntensity(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return 0.0f;
			return material->GetWorkFlow() == MetallicRoughness
				? material->GetMetallicRoughData()
					.emissive_intensity
				: material->GetSpecularGlossData()
					.emissive_intensity;
		}

		void SetMaterialEmissiveIntensity(
			float value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return;
			value = std::max(value, 0.0f);
			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.emissive_intensity = value;
				material->SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.emissive_intensity = value;
				material->SetSpecularGlossData(data);
			}
		}

		float GetMaterialNormalStrength(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return 0.0f;
			return material->GetWorkFlow() == MetallicRoughness
				? material->GetMetallicRoughData().normal_strength
				: material->GetSpecularGlossData().normal_strength;
		}

		void SetMaterialNormalStrength(
			float value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return;
			value = std::max(value, 0.0f);
			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.normal_strength = value;
				material->SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.normal_strength = value;
				material->SetSpecularGlossData(data);
			}
		}

		float GetMaterialOcclusionStrength(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return 0.0f;
			return material->GetWorkFlow() == MetallicRoughness
				? material->GetMetallicRoughData()
					.occlusion_strength
				: material->GetSpecularGlossData()
					.occlusion_strength;
		}

		void SetMaterialOcclusionStrength(
			float value,
			ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			if (!material)
				return;
			value = std::max(value, 0.0f);
			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.occlusion_strength = value;
				material->SetMetallicRoughData(data);
			}
			else
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.occlusion_strength = value;
				material->SetSpecularGlossData(data);
			}
		}

		float GetMaterialUvTilingX(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material ? material->GetUVTiling().x : 0.0f;
		}

		void SetMaterialUvTilingX(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
			{
				float2 tiling = material->GetUVTiling();
				tiling.x = value;
				material->SetUVTiling(tiling);
			}
		}

		float GetMaterialUvTilingY(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material ? material->GetUVTiling().y : 0.0f;
		}

		void SetMaterialUvTilingY(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
			{
				float2 tiling = material->GetUVTiling();
				tiling.y = value;
				material->SetUVTiling(tiling);
			}
		}

		float GetMaterialUvOffsetX(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material ? material->GetUVOffset().x : 0.0f;
		}

		void SetMaterialUvOffsetX(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
			{
				float2 offset = material->GetUVOffset();
				offset.x = value;
				material->SetUVOffset(offset);
			}
		}

		float GetMaterialUvOffsetY(
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material ? material->GetUVOffset().y : 0.0f;
		}

		void SetMaterialUvOffsetY(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceMaterial* material = ResolveMaterial(reference))
			{
				float2 offset = material->GetUVOffset();
				offset.y = value;
				material->SetUVOffset(offset);
			}
		}

		UID GetMaterialTextureId(
			const ResourceMaterial& material,
			MetallicRoughTextures metallicSlot,
			SpecularGlossTextures specularSlot,
			bool supportedBySpecular)
		{
			if (material.GetWorkFlow() == MetallicRoughness)
				return material.GetMetallicRoughData()
					.textures[metallicSlot];
			return supportedBySpecular
				? material.GetSpecularGlossData()
					.textures[specularSlot]
				: 0;
		}

		ScriptResourceReference* GetMaterialTexture(
			MetallicRoughTextures metallicSlot,
			SpecularGlossTextures specularSlot,
			bool supportedBySpecular,
			const ScriptResourceReference* reference)
		{
			ResourceMaterial* material = ResolveMaterial(reference);
			return material
				? GetScriptResourceReference(
					GetMaterialTextureId(
						*material,
						metallicSlot,
						specularSlot,
						supportedBySpecular),
					Resource::texture)
				: nullptr;
		}

		void SetMaterialTexture(
			MetallicRoughTextures metallicSlot,
			SpecularGlossTextures specularSlot,
			bool supportedBySpecular,
			ScriptResourceReference* texture,
			ScriptResourceReference* materialReference)
		{
			ResourceMaterial* material =
				ResolveMaterial(materialReference);
			if (!material)
				return;
			const UID textureId = texture
				? ResolveScriptResourceId(texture, Resource::texture)
				: 0;
			if (texture && textureId == 0)
				return;

			if (material->GetWorkFlow() == MetallicRoughness)
			{
				MetallicRoughData data =
					material->GetMetallicRoughData();
				data.textures[metallicSlot] = textureId;
				material->SetMetallicRoughData(data);
			}
			else if (supportedBySpecular)
			{
				SpecularGlossData data =
					material->GetSpecularGlossData();
				data.textures[specularSlot] = textureId;
				material->SetSpecularGlossData(data);
			}
		}

#define EGE_MATERIAL_TEXTURE_ACCESSORS(                                  \
	Name, MrSlot, SgSlot, SupportsSg)                                    \
	ScriptResourceReference* Get##Name(                                  \
		const ScriptResourceReference* reference)                        \
	{                                                                   \
		return GetMaterialTexture(                                      \
			MrSlot, SgSlot, SupportsSg, reference);                     \
	}                                                                   \
	void Set##Name(                                                      \
		ScriptResourceReference* texture,                                \
		ScriptResourceReference* reference)                              \
	{                                                                   \
		SetMaterialTexture(                                             \
			MrSlot, SgSlot, SupportsSg, texture, reference);            \
	}

		EGE_MATERIAL_TEXTURE_ACCESSORS(
			MaterialBaseColorTexture,
			MR_TextureBaseColor,
			SG_TextureDiffuse,
			true)
		EGE_MATERIAL_TEXTURE_ACCESSORS(
			MaterialNormalTexture,
			MR_TextureNormal,
			SG_TextureNormal,
			true)
		EGE_MATERIAL_TEXTURE_ACCESSORS(
			MaterialEmissiveTexture,
			MR_TextureEmissive,
			SG_TextureEmissive,
			true)
		EGE_MATERIAL_TEXTURE_ACCESSORS(
			MaterialOcclusionTexture,
			MR_TextureOcclusion,
			SG_TextureOcclusion,
			true)
		EGE_MATERIAL_TEXTURE_ACCESSORS(
			MaterialMetallicRoughnessTexture,
			MR_TextureMetallicRough,
			SG_TextureSpecular,
			false)

#undef EGE_MATERIAL_TEXTURE_ACCESSORS

		std::uint32_t GetMeshVertexCount(
			const ScriptResourceReference* reference)
		{
			ResourceMesh* mesh = ResolveTypedResource<ResourceMesh>(
				reference, Resource::mesh, "Mesh");
			return mesh ? mesh->GetNumVertices() : 0;
		}

		std::uint32_t GetMeshIndexCount(
			const ScriptResourceReference* reference)
		{
			ResourceMesh* mesh = ResolveTypedResource<ResourceMesh>(
				reference, Resource::mesh, "Mesh");
			return mesh ? mesh->GetNumIndices() : 0;
		}

		std::uint32_t GetMeshMorphTargetCount(
			const ScriptResourceReference* reference)
		{
			ResourceMesh* mesh = ResolveTypedResource<ResourceMesh>(
				reference, Resource::mesh, "Mesh");
			return mesh ? mesh->GetNumMorphTargets() : 0;
		}

		ScriptVector3 GetMeshBoundsCenter(
			const ScriptResourceReference* reference)
		{
			ResourceMesh* mesh = ResolveTypedResource<ResourceMesh>(
				reference, Resource::mesh, "Mesh");
			if (!mesh)
				return {};
			const float3 center = mesh->bbox.CenterPoint();
			return {center.x, center.y, center.z};
		}

		ScriptVector3 GetMeshBoundsSize(
			const ScriptResourceReference* reference)
		{
			ResourceMesh* mesh = ResolveTypedResource<ResourceMesh>(
				reference, Resource::mesh, "Mesh");
			if (!mesh)
				return {};
			const float3 size = mesh->bbox.Size();
			return {size.x, size.y, size.z};
		}

		ResourceTexture* ResolveTexture(
			const ScriptResourceReference* reference)
		{
			return ResolveTypedResource<ResourceTexture>(
				reference, Resource::texture, "Texture");
		}

		std::uint32_t GetTextureWidth(
			const ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			return texture ? texture->GetMetadata().width : 0;
		}

		std::uint32_t GetTextureHeight(
			const ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			return texture ? texture->GetMetadata().height : 0;
		}

		std::uint32_t GetTextureDepth(
			const ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			return texture ? texture->GetMetadata().depth : 0;
		}

		std::uint32_t GetTextureMipCount(
			const ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			return texture ? texture->GetMetadata().mipCount : 0;
		}

		std::string GetTextureFormat(
			const ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			return texture ? texture->GetFormatStr() : std::string();
		}

		int GetTextureColorSpace(
			const ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			return texture
				? static_cast<int>(texture->GetColorSpace())
				: static_cast<int>(ColorSpace_gamma);
		}

		void SetTextureColorSpace(
			int value,
			ScriptResourceReference* reference)
		{
			ResourceTexture* texture = ResolveTexture(reference);
			if (!texture)
				return;
			const int clamped = std::clamp(
				value,
				static_cast<int>(ColorSpace_gamma),
				static_cast<int>(ColorSpace_linear));
			texture->SetColorSpace(
				static_cast<ColorSpace>(clamped));
		}

		ResourceAudio* ResolveAudio(
			const ScriptResourceReference* reference)
		{
			return ResolveTypedResource<ResourceAudio>(
				reference, Resource::audio, "AudioClip");
		}

		int GetAudioFormat(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio
				? static_cast<int>(audio->GetFormat())
				: static_cast<int>(ResourceAudio::unknown);
		}

		bool GetAudioLoop(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio && audio->GetLoop();
		}

		void SetAudioLoop(
			bool value,
			ScriptResourceReference* reference)
		{
			if (ResourceAudio* audio = ResolveAudio(reference))
				audio->SetLoop(value);
		}

		float GetAudioVolume(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio ? audio->GetVolume() : 0.0f;
		}

		void SetAudioVolume(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceAudio* audio = ResolveAudio(reference))
				audio->SetVolume(value);
		}

		float GetAudioPitch(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio ? audio->GetPitch() : 0.0f;
		}

		void SetAudioPitch(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceAudio* audio = ResolveAudio(reference))
				audio->SetPitch(value);
		}

		bool GetAudioSpatial(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio && audio->GetSpatial();
		}

		void SetAudioSpatial(
			bool value,
			ScriptResourceReference* reference)
		{
			if (ResourceAudio* audio = ResolveAudio(reference))
				audio->SetSpatial(value);
		}

		float GetAudioMinimumDistance(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio ? audio->GetMinimumDistance() : 0.0f;
		}

		void SetAudioMinimumDistance(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceAudio* audio = ResolveAudio(reference))
			{
				audio->SetDistanceRange(
					value, audio->GetMaximumDistance());
			}
		}

		float GetAudioMaximumDistance(
			const ScriptResourceReference* reference)
		{
			ResourceAudio* audio = ResolveAudio(reference);
			return audio ? audio->GetMaximumDistance() : 0.0f;
		}

		void SetAudioMaximumDistance(
			float value,
			ScriptResourceReference* reference)
		{
			if (ResourceAudio* audio = ResolveAudio(reference))
			{
				audio->SetDistanceRange(
					audio->GetMinimumDistance(), value);
			}
		}

		float GetAnimationDuration(
			const ScriptResourceReference* reference)
		{
			ResourceAnimation* animation =
				ResolveLoadedResource<ResourceAnimation>(
					reference,
					Resource::animation,
					"AnimationClip");
			return animation ? animation->GetDuration() : 0.0f;
		}

		std::uint32_t GetAnimationChannelCount(
			const ScriptResourceReference* reference)
		{
			ResourceAnimation* animation =
				ResolveLoadedResource<ResourceAnimation>(
					reference,
					Resource::animation,
					"AnimationClip");
			return animation ? animation->GetNumChannels() : 0;
		}

		std::uint32_t GetAnimationMorphChannelCount(
			const ScriptResourceReference* reference)
		{
			ResourceAnimation* animation =
				ResolveLoadedResource<ResourceAnimation>(
					reference,
					Resource::animation,
					"AnimationClip");
			return animation
				? animation->GetNumMorphChannels()
				: 0;
		}

		bool HasAnimationChannel(
			const std::string& name,
			const ScriptResourceReference* reference)
		{
			ResourceAnimation* animation =
				ResolveLoadedResource<ResourceAnimation>(
					reference,
					Resource::animation,
					"AnimationClip");
			return animation &&
				animation->GetChannel(name) != nullptr;
		}

		bool HasAnimationMorphChannel(
			const std::string& name,
			const ScriptResourceReference* reference)
		{
			ResourceAnimation* animation =
				ResolveLoadedResource<ResourceAnimation>(
					reference,
					Resource::animation,
					"AnimationClip");
			return animation &&
				animation->GetMorphChannel(name) != nullptr;
		}

		std::uint32_t GetModelNodeCount(
			const ScriptResourceReference* reference)
		{
			ResourceModel* model = ResolveTypedResource<ResourceModel>(
				reference, Resource::model, "Model");
			return model ? model->GetNumNodes() : 0;
		}

		std::uint32_t GetModelSkinCount(
			const ScriptResourceReference* reference)
		{
			ResourceModel* model = ResolveTypedResource<ResourceModel>(
				reference, Resource::model, "Model");
			return model ? model->GetNumSkins() : 0;
		}

		ResourceStateMachine* ResolveLoadedStateMachine(
			const ScriptResourceReference* reference)
		{
			return ResolveLoadedResource<ResourceStateMachine>(
				reference,
				Resource::state_machine,
				"AnimationStateMachine");
		}

		std::uint32_t GetStateMachineClipCount(
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine ? stateMachine->GetNumClips() : 0;
		}

		std::uint32_t GetStateMachineNodeCount(
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine ? stateMachine->GetNumNodes() : 0;
		}

		std::uint32_t GetStateMachineTransitionCount(
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine
				? stateMachine->GetNumTransitions()
				: 0;
		}

		std::uint32_t GetStateMachineDefaultNode(
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine
				? stateMachine->GetDefaultNode()
				: 0;
		}

		void SetStateMachineDefaultNode(
			std::uint32_t value,
			ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			if (!stateMachine)
				return;
			if (value >= stateMachine->GetNumNodes() &&
				stateMachine->GetNumNodes() != 0)
			{
				SetScriptException(
					"The default state node index is out of range.");
				return;
			}
			stateMachine->SetDefaultNode(value);
		}

		bool ValidateStateMachineIndex(
			std::uint32_t index,
			std::uint32_t count,
			const char* collection)
		{
			if (index < count)
				return true;
			const std::string message =
				std::string("The animation state machine ") +
				collection + " index is out of range.";
			SetScriptException(message.c_str());
			return false;
		}

		std::string ToScriptString(const HashString& value)
		{
			return value ? value.C_str() : std::string();
		}

		std::string GetStateMachineClipName(
			std::uint32_t index,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine &&
				ValidateStateMachineIndex(
					index, stateMachine->GetNumClips(), "clip")
				? ToScriptString(stateMachine->GetClipName(index))
				: std::string();
		}

		ScriptResourceReference* GetStateMachineClip(
			std::uint32_t index,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			if (!stateMachine ||
				!ValidateStateMachineIndex(
					index, stateMachine->GetNumClips(), "clip"))
			{
				return nullptr;
			}
			return GetScriptResourceReference(
				stateMachine->GetClipRes(index),
				Resource::animation);
		}

		bool GetStateMachineClipLoop(
			std::uint32_t index,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine &&
				ValidateStateMachineIndex(
					index, stateMachine->GetNumClips(), "clip") &&
				stateMachine->GetClipLoop(index);
		}

		int FindStateMachineClip(
			const std::string& name,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			if (!stateMachine)
				return -1;
			const std::uint32_t index =
				stateMachine->FindClip(HashString(name.c_str()));
			return index < stateMachine->GetNumClips()
				? static_cast<int>(index)
				: -1;
		}

		std::string GetStateMachineStateName(
			std::uint32_t index,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine &&
				ValidateStateMachineIndex(
					index, stateMachine->GetNumNodes(), "state")
				? ToScriptString(stateMachine->GetNodeName(index))
				: std::string();
		}

		std::string GetStateMachineStateClipName(
			std::uint32_t index,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine &&
				ValidateStateMachineIndex(
					index, stateMachine->GetNumNodes(), "state")
				? ToScriptString(stateMachine->GetNodeClip(index))
				: std::string();
		}

		int FindStateMachineState(
			const std::string& name,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			if (!stateMachine)
				return -1;
			const std::uint32_t index =
				stateMachine->FindNode(HashString(name.c_str()));
			return index < stateMachine->GetNumNodes()
				? static_cast<int>(index)
				: -1;
		}

		bool HasStateMachineState(
			const std::string& name,
			const ScriptResourceReference* reference)
		{
			return FindStateMachineState(name, reference) >= 0;
		}

		std::string GetStateMachineDefaultState(
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			if (!stateMachine ||
				stateMachine->GetDefaultNode() >=
					stateMachine->GetNumNodes())
			{
				return {};
			}
			return ToScriptString(
				stateMachine->GetNodeName(
					stateMachine->GetDefaultNode()));
		}

#define EGE_STATE_MACHINE_TRANSITION_STRING(Name, Getter) \
		std::string GetStateMachineTransition##Name( \
			std::uint32_t index, \
			const ScriptResourceReference* reference) \
		{ \
			ResourceStateMachine* stateMachine = \
				ResolveLoadedStateMachine(reference); \
			return stateMachine && \
				ValidateStateMachineIndex( \
					index, stateMachine->GetNumTransitions(), \
					"transition") \
				? ToScriptString(stateMachine->Getter(index)) \
				: std::string(); \
		}

		EGE_STATE_MACHINE_TRANSITION_STRING(
			Source, GetTransitionSource)
		EGE_STATE_MACHINE_TRANSITION_STRING(
			Target, GetTransitionTarget)
		EGE_STATE_MACHINE_TRANSITION_STRING(
			Trigger, GetTransitionTrigger)

#undef EGE_STATE_MACHINE_TRANSITION_STRING

		std::uint32_t GetStateMachineTransitionBlend(
			std::uint32_t index,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			return stateMachine &&
				ValidateStateMachineIndex(
					index,
					stateMachine->GetNumTransitions(),
					"transition")
				? stateMachine->GetTransitionBlend(index)
				: 0;
		}

		bool HasStateMachineTrigger(
			const std::string& trigger,
			const ScriptResourceReference* reference)
		{
			ResourceStateMachine* stateMachine =
				ResolveLoadedStateMachine(reference);
			if (!stateMachine)
				return false;
			const HashString expected(trigger.c_str());
			for (std::uint32_t index = 0;
				index < stateMachine->GetNumTransitions();
				++index)
			{
				if (stateMachine->GetTransitionTrigger(index) ==
					expected)
				{
					return true;
				}
			}
			return false;
		}

		bool RegisterResourceHandle(
			asIScriptEngine& engine,
			const char* type,
			std::string& error)
		{
			const bool registered =
				engine.RegisterObjectType(type, 0, asOBJ_REF) >= 0 &&
				engine.RegisterObjectBehaviour(
					type, asBEHAVE_ADDREF, "void f()",
					asMETHOD(ScriptResourceReference, AddRef),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectBehaviour(
					type, asBEHAVE_RELEASE, "void f()",
					asMETHOD(ScriptResourceReference, Release),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "uint64 get_id() const property",
					asMETHOD(
						ScriptResourceReference,
						GetResourceId),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool get_valid() const property",
					asMETHOD(
						ScriptResourceReference,
						IsValid),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "string get_name() const property",
					asFUNCTION(GetResourceName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type,
					"void set_name(const string &in) property",
					asFUNCTION(SetResourceName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "string get_path() const property",
					asFUNCTION(GetResourcePath),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type,
					"string get_typeName() const property",
					asFUNCTION(GetResourceTypeName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool get_loaded() const property",
					asFUNCTION(GetResourceLoaded),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type,
					"uint get_referenceCount() const property",
					asFUNCTION(GetResourceReferenceCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool Save()",
					asFUNCTION(SaveResource),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type,
					("bool opEquals(const " +
						std::string(type) +
						"@+ other) const").c_str(),
					asFUNCTION(ResourcesEqual),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error =
				"Could not register the '" +
				std::string(type) + "' asset handle.";
			return false;
		}

		bool RegisterMaterialApi(
			asIScriptEngine& engine)
		{
			return
				engine.RegisterEnum("MaterialWorkflow") >= 0 &&
				engine.RegisterEnumValue(
					"MaterialWorkflow",
					"SpecularGlossiness",
					SpecularGlossiness) >= 0 &&
				engine.RegisterEnumValue(
					"MaterialWorkflow",
					"MetallicRoughness",
					MetallicRoughness) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"MaterialWorkflow get_workflow() const property",
					asFUNCTION(GetMaterialWorkflow),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"bool get_doubleSided() const property",
					asFUNCTION(GetMaterialDoubleSided),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_doubleSided(bool) property",
					asFUNCTION(SetMaterialDoubleSided),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"bool get_planarReflections() const property",
					asFUNCTION(GetMaterialPlanarReflections),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_planarReflections(bool) property",
					asFUNCTION(SetMaterialPlanarReflections),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_alphaCutoff() const property",
					asFUNCTION(GetMaterialAlphaCutoff),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_alphaCutoff(float) property",
					asFUNCTION(SetMaterialAlphaCutoff),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Color get_baseColor() const property",
					asFUNCTION(GetMaterialBaseColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_baseColor(const Color &in) property",
					asFUNCTION(SetMaterialBaseColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_metallic() const property",
					asFUNCTION(GetMaterialMetallic),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_metallic(float) property",
					asFUNCTION(SetMaterialMetallic),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_roughness() const property",
					asFUNCTION(GetMaterialRoughness),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_roughness(float) property",
					asFUNCTION(SetMaterialRoughness),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Color get_specularColor() const property",
					asFUNCTION(GetMaterialSpecularColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_specularColor(const Color &in) property",
					asFUNCTION(SetMaterialSpecularColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Color get_emissiveColor() const property",
					asFUNCTION(GetMaterialEmissiveColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_emissiveColor(const Color &in) property",
					asFUNCTION(SetMaterialEmissiveColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_emissiveIntensity() const property",
					asFUNCTION(GetMaterialEmissiveIntensity),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_emissiveIntensity(float) property",
					asFUNCTION(SetMaterialEmissiveIntensity),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_normalStrength() const property",
					asFUNCTION(GetMaterialNormalStrength),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_normalStrength(float) property",
					asFUNCTION(SetMaterialNormalStrength),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_occlusionStrength() const property",
					asFUNCTION(GetMaterialOcclusionStrength),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_occlusionStrength(float) property",
					asFUNCTION(SetMaterialOcclusionStrength),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_uvTilingX() const property",
					asFUNCTION(GetMaterialUvTilingX),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_uvTilingX(float) property",
					asFUNCTION(SetMaterialUvTilingX),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_uvTilingY() const property",
					asFUNCTION(GetMaterialUvTilingY),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_uvTilingY(float) property",
					asFUNCTION(SetMaterialUvTilingY),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_uvOffsetX() const property",
					asFUNCTION(GetMaterialUvOffsetX),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_uvOffsetX(float) property",
					asFUNCTION(SetMaterialUvOffsetX),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"float get_uvOffsetY() const property",
					asFUNCTION(GetMaterialUvOffsetY),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_uvOffsetY(float) property",
					asFUNCTION(SetMaterialUvOffsetY),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Texture@ get_baseColorTexture() const property",
					asFUNCTION(GetMaterialBaseColorTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_baseColorTexture(Texture@+) property",
					asFUNCTION(SetMaterialBaseColorTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Texture@ get_normalTexture() const property",
					asFUNCTION(GetMaterialNormalTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_normalTexture(Texture@+) property",
					asFUNCTION(SetMaterialNormalTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Texture@ get_emissiveTexture() const property",
					asFUNCTION(GetMaterialEmissiveTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_emissiveTexture(Texture@+) property",
					asFUNCTION(SetMaterialEmissiveTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Texture@ get_occlusionTexture() const property",
					asFUNCTION(GetMaterialOcclusionTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_occlusionTexture(Texture@+) property",
					asFUNCTION(SetMaterialOcclusionTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"Texture@ get_metallicRoughnessTexture() const property",
					asFUNCTION(
						GetMaterialMetallicRoughnessTexture),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Material",
					"void set_metallicRoughnessTexture(Texture@+) property",
					asFUNCTION(
						SetMaterialMetallicRoughnessTexture),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterMeshApi(asIScriptEngine& engine)
		{
			return
				engine.RegisterObjectMethod(
					"Mesh",
					"uint get_vertexCount() const property",
					asFUNCTION(GetMeshVertexCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Mesh",
					"uint get_indexCount() const property",
					asFUNCTION(GetMeshIndexCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Mesh",
					"uint get_morphTargetCount() const property",
					asFUNCTION(GetMeshMorphTargetCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Mesh",
					"Vector3 get_boundsCenter() const property",
					asFUNCTION(GetMeshBoundsCenter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Mesh",
					"Vector3 get_boundsSize() const property",
					asFUNCTION(GetMeshBoundsSize),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterTextureApi(asIScriptEngine& engine)
		{
			return
				engine.RegisterEnum("TextureColorSpace") >= 0 &&
				engine.RegisterEnumValue(
					"TextureColorSpace",
					"Gamma",
					ColorSpace_gamma) >= 0 &&
				engine.RegisterEnumValue(
					"TextureColorSpace",
					"Linear",
					ColorSpace_linear) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture", "uint get_width() const property",
					asFUNCTION(GetTextureWidth),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture", "uint get_height() const property",
					asFUNCTION(GetTextureHeight),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture", "uint get_depth() const property",
					asFUNCTION(GetTextureDepth),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture", "uint get_mipCount() const property",
					asFUNCTION(GetTextureMipCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture",
					"string get_format() const property",
					asFUNCTION(GetTextureFormat),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture",
					"TextureColorSpace get_colorSpace() const property",
					asFUNCTION(GetTextureColorSpace),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Texture",
					"void set_colorSpace(TextureColorSpace) property",
					asFUNCTION(SetTextureColorSpace),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterAudioApi(asIScriptEngine& engine)
		{
			return
				engine.RegisterEnum("AudioClipFormat") >= 0 &&
				engine.RegisterEnumValue(
					"AudioClipFormat", "Sample",
					ResourceAudio::sample) >= 0 &&
				engine.RegisterEnumValue(
					"AudioClipFormat", "Stream",
					ResourceAudio::stream) >= 0 &&
				engine.RegisterEnumValue(
					"AudioClipFormat", "Unknown",
					ResourceAudio::unknown) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip",
					"AudioClipFormat get_format() const property",
					asFUNCTION(GetAudioFormat),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "bool get_loop() const property",
					asFUNCTION(GetAudioLoop),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "void set_loop(bool) property",
					asFUNCTION(SetAudioLoop),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "float get_volume() const property",
					asFUNCTION(GetAudioVolume),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "void set_volume(float) property",
					asFUNCTION(SetAudioVolume),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "float get_pitch() const property",
					asFUNCTION(GetAudioPitch),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "void set_pitch(float) property",
					asFUNCTION(SetAudioPitch),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "bool get_spatial() const property",
					asFUNCTION(GetAudioSpatial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip", "void set_spatial(bool) property",
					asFUNCTION(SetAudioSpatial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip",
					"float get_minimumDistance() const property",
					asFUNCTION(GetAudioMinimumDistance),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip",
					"void set_minimumDistance(float) property",
					asFUNCTION(SetAudioMinimumDistance),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip",
					"float get_maximumDistance() const property",
					asFUNCTION(GetAudioMaximumDistance),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioClip",
					"void set_maximumDistance(float) property",
					asFUNCTION(SetAudioMaximumDistance),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterReadOnlyAssetApis(asIScriptEngine& engine)
		{
			return
				engine.RegisterObjectMethod(
					"AnimationClip",
					"float get_duration() const property",
					asFUNCTION(GetAnimationDuration),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationClip",
					"uint get_channelCount() const property",
					asFUNCTION(GetAnimationChannelCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationClip",
					"uint get_morphChannelCount() const property",
					asFUNCTION(GetAnimationMorphChannelCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationClip",
					"bool HasChannel("
					"const string &in name) const",
					asFUNCTION(HasAnimationChannel),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationClip",
					"bool HasMorphChannel("
					"const string &in name) const",
					asFUNCTION(HasAnimationMorphChannel),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Model", "uint get_nodeCount() const property",
					asFUNCTION(GetModelNodeCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Model", "uint get_skinCount() const property",
					asFUNCTION(GetModelSkinCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"uint get_clipCount() const property",
					asFUNCTION(GetStateMachineClipCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"uint get_nodeCount() const property",
					asFUNCTION(GetStateMachineNodeCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"uint get_transitionCount() const property",
					asFUNCTION(GetStateMachineTransitionCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"uint get_defaultNode() const property",
					asFUNCTION(GetStateMachineDefaultNode),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"void set_defaultNode(uint) property",
					asFUNCTION(SetStateMachineDefaultNode),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string get_defaultState() const property",
					asFUNCTION(GetStateMachineDefaultState),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string GetClipName(uint index) const",
					asFUNCTION(GetStateMachineClipName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"AnimationClip@ GetClip(uint index) const",
					asFUNCTION(GetStateMachineClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"bool IsClipLooping(uint index) const",
					asFUNCTION(GetStateMachineClipLoop),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"int FindClip("
					"const string &in name) const",
					asFUNCTION(FindStateMachineClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string GetStateName(uint index) const",
					asFUNCTION(GetStateMachineStateName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string GetStateClipName(uint index) const",
					asFUNCTION(GetStateMachineStateClipName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"int FindState("
					"const string &in name) const",
					asFUNCTION(FindStateMachineState),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"bool HasState("
					"const string &in name) const",
					asFUNCTION(HasStateMachineState),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string GetTransitionSource("
					"uint index) const",
					asFUNCTION(GetStateMachineTransitionSource),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string GetTransitionTarget("
					"uint index) const",
					asFUNCTION(GetStateMachineTransitionTarget),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"string GetTransitionTrigger("
					"uint index) const",
					asFUNCTION(GetStateMachineTransitionTrigger),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"uint GetTransitionBlendMilliseconds("
					"uint index) const",
					asFUNCTION(GetStateMachineTransitionBlend),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AnimationStateMachine",
					"bool HasTrigger("
					"const string &in trigger) const",
					asFUNCTION(HasStateMachineTrigger),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		struct ResourceApi
		{
			const char* scriptType;
		};

		constexpr std::array<ResourceApi, 7> ResourceApis{{
			{"Model"},
			{"Material"},
			{"Texture"},
			{"Mesh"},
			{"AudioClip"},
			{"AnimationClip"},
			{"AnimationStateMachine"}
		}};

#define EGE_TYPED_RESOURCE_FUNCTIONS(Name, Type)                         \
		ScriptResourceReference* Get##Name(std::uint64_t id)             \
		{                                                                \
			return GetScriptResourceReference(id, Type);                \
		}                                                                \
		ScriptResourceReference* Find##Name(const std::string& path)      \
		{                                                                \
			return FindScriptResourceReference(path, Type);             \
		}

		EGE_TYPED_RESOURCE_FUNCTIONS(Model, Resource::model)
		EGE_TYPED_RESOURCE_FUNCTIONS(Material, Resource::material)
		EGE_TYPED_RESOURCE_FUNCTIONS(Texture, Resource::texture)
		EGE_TYPED_RESOURCE_FUNCTIONS(Mesh, Resource::mesh)
		EGE_TYPED_RESOURCE_FUNCTIONS(AudioClip, Resource::audio)
		EGE_TYPED_RESOURCE_FUNCTIONS(
			AnimationClip, Resource::animation)
		EGE_TYPED_RESOURCE_FUNCTIONS(
			AnimationStateMachine, Resource::state_machine)

#undef EGE_TYPED_RESOURCE_FUNCTIONS
	}

	ScriptResourceReference* GetScriptResourceReference(
		std::uint64_t id,
		Resource::Type type)
	{
		if (!App || !App->resources || id == 0)
			return nullptr;
		Resource* resource = App->resources->Get(id);
		return resource && resource->GetType() == type
			? MakeResourceReference(id, static_cast<int>(type))
			: nullptr;
	}

	ScriptResourceReference* FindScriptResourceReference(
		const std::string& path,
		Resource::Type type)
	{
		if (!App || !App->resources || path.empty())
			return nullptr;
		const Resource* resource =
			App->resources->FindResourceBySourceFile(type, path);
		return resource
			? GetScriptResourceReference(resource->GetUID(), type)
			: nullptr;
	}

	UID ResolveScriptResourceId(
		const ScriptResourceReference* reference,
		Resource::Type expectedType)
	{
		Resource* resource =
			reference ? reference->Resolve() : nullptr;
		if (!resource || resource->GetType() != expectedType)
		{
			SetScriptException(
				"The asset reference is no longer valid or has the wrong type.");
			return 0;
		}
		return resource->GetUID();
	}

	bool RegisterScriptAssetApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		for (const ResourceApi& api : ResourceApis)
		{
			if (!RegisterResourceHandle(
					engine, api.scriptType, error))
			{
				return false;
			}
		}

		if (!RegisterMaterialApi(engine) ||
			!RegisterMeshApi(engine) ||
			!RegisterTextureApi(engine) ||
			!RegisterAudioApi(engine) ||
			!RegisterReadOnlyAssetApis(engine))
		{
			error = "Could not register the typed asset properties.";
			return false;
		}

		engine.SetDefaultNamespace("Resources");
		const bool registered =
			engine.RegisterGlobalFunction(
				"Mesh@ GetMesh(uint64 id)",
				asFUNCTION(GetMesh),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Mesh@ FindMesh(const string &in assetPath)",
				asFUNCTION(FindMesh),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Material@ GetMaterial(uint64 id)",
				asFUNCTION(GetMaterial),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Material@ FindMaterial(const string &in assetPath)",
				asFUNCTION(FindMaterial),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Texture@ GetTexture(uint64 id)",
				asFUNCTION(GetTexture),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Texture@ FindTexture(const string &in assetPath)",
				asFUNCTION(FindTexture),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"AudioClip@ GetAudioClip(uint64 id)",
				asFUNCTION(GetAudioClip),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"AudioClip@ FindAudioClip(const string &in assetPath)",
				asFUNCTION(FindAudioClip),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"AnimationClip@ GetAnimationClip(uint64 id)",
				asFUNCTION(GetAnimationClip),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"AnimationClip@ FindAnimationClip("
					"const string &in assetPath)",
				asFUNCTION(FindAnimationClip),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Model@ GetModel(uint64 id)",
				asFUNCTION(GetModel),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"Model@ FindModel(const string &in assetPath)",
				asFUNCTION(FindModel),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"AnimationStateMachine@ GetAnimationStateMachine("
					"uint64 id)",
				asFUNCTION(GetAnimationStateMachine),
				asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"AnimationStateMachine@ FindAnimationStateMachine("
					"const string &in assetPath)",
				asFUNCTION(FindAnimationStateMachine),
				asCALL_CDECL) >= 0;
		engine.SetDefaultNamespace("");
		if (!registered)
		{
			error = "Could not register the Resources API.";
			return false;
		}
		return true;
	}
}
