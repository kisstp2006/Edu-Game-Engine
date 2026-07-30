#include "ScriptComponentBindings.h"

#include "ScriptAssetBindings.h"
#include "ScriptMath.h"
#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../Component.h"
#include "../ComponentAnimation.h"
#include "../ComponentAudioSource.h"
#include "../ComponentReziAudioEmitter.h"
#include "../ComponentReziAudioListener.h"
#include "../ComponentCamera.h"
#include "../ComponentCollider.h"
#include "../ComponentMeshRenderer.h"
#include "../ComponentRigidBody.h"
#include "../ComponentScript.h"
#include "../GameObject.h"
#include "../ModuleResources.h"
#include "../ModuleRenderer3D.h"
#include "../ModuleWindow.h"
#include "../Resource.h"
#include "../SceneViewport.h"
#include "../Viewport.h"

#include <angelscript.h>
#include <scriptarray/scriptarray.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#ifdef GetObject
#undef GetObject
#endif

namespace EGE
{
	namespace
	{
		struct ComponentBinding
		{
			const char* scriptType = nullptr;
			Component::Types engineType = Component::Unknown;
			bool creatable = false;
			bool removable = false;
			bool allowMultiple = false;
		};

		constexpr std::array<ComponentBinding, 9> ComponentBindings{{
			{"MeshRenderer", Component::MeshRenderer, true, true, true},
			{"Camera", Component::Camera, true, true, false},
			{"Animation", Component::Animation, true, true, false},
			{"AudioSource", Component::AudioSource, true, true, true},
			{"AudioListener", Component::AudioListener, true, true, false},
			{"RigidBody", Component::RigidBody, true, true, false},
			{"Collider", Component::Collider, true, true, true},
			{"ReziAudioEmitter", Component::ReziAudioEmitter, true, true, true},
			{"ReziAudioListener", Component::ReziAudioListener, true, true, false}
		}};

		void SetScriptException(const char* message)
		{
			if (asIScriptContext* context = asGetActiveContext())
				context->SetException(message);
		}

		const ComponentBinding* FindBinding(
			asIScriptEngine& engine,
			int typeId)
		{
			asITypeInfo* type = engine.GetTypeInfoById(typeId);
			if (!type)
				return nullptr;

			for (const ComponentBinding& binding : ComponentBindings)
			{
				if (binding.scriptType == std::string(type->GetName()))
					return &binding;
			}
			return nullptr;
		}

		const ComponentBinding* GetTemplateBinding(
			asIScriptGeneric& generic)
		{
			asIScriptFunction* function = generic.GetFunction();
			if (!function || function->GetSubTypeCount() != 1)
				return nullptr;
			return FindBinding(
				*generic.GetEngine(),
				function->GetSubTypeId());
		}

		asITypeInfo* GetTemplateType(
			asIScriptGeneric& generic)
		{
			asIScriptFunction* function = generic.GetFunction();
			return function && function->GetSubTypeCount() == 1
				? generic.GetEngine()->GetTypeInfoById(
					function->GetSubTypeId())
				: nullptr;
		}

		bool IsScriptComponentType(const asITypeInfo* type)
		{
			for (const asITypeInfo* current = type;
				current;
				current = current->GetBaseType())
			{
				if (std::string_view(current->GetName()) ==
					"EGEBehaviour")
				{
					return current != type;
				}
			}
			return false;
		}

		GameObject* ResolveOwner(
			const ScriptGameObjectReference* reference)
		{
			GameObject* owner = reference ? reference->Resolve() : nullptr;
			if (!owner)
			{
				SetScriptException(
					"The GameObject reference is no longer valid.");
			}
			return owner;
		}

		Component* FindFirstComponent(
			GameObject& owner,
			const ComponentBinding& binding)
		{
			for (Component* component : owner.components)
			{
				if (component &&
					!component->flag_for_removal &&
					component->GetType() == binding.engineType)
				{
					return component;
				}
			}
			return nullptr;
		}

		ScriptComponentReference* MakeReference(
			GameObject& owner,
			Component* component)
		{
			return component
				? MakeComponentReference(
					owner.GetUID(), component->GetUID())
				: nullptr;
		}

		asIScriptObject* FindFirstScriptComponent(
			GameObject& owner,
			const asITypeInfo& requestedType)
		{
			for (Component* component : owner.components)
			{
				if (!component ||
					component->flag_for_removal ||
					component->GetType() != Component::Script)
				{
					continue;
				}

				auto* script =
					static_cast<ComponentScript*>(component);
				if (asIScriptObject* object =
					script->AcquireScriptObject(requestedType))
				{
					return object;
				}
			}
			return nullptr;
		}

		void GetComponentTemplate(asIScriptGeneric* generic)
		{
			*static_cast<void**>(
				generic->GetAddressOfReturnLocation()) = nullptr;
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			asITypeInfo* requestedType =
				GetTemplateType(*generic);
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			if (!binding &&
				!IsScriptComponentType(requestedType))
			{
				SetScriptException(
					"The requested type is not an engine or script component.");
				return;
			}

			GameObject* owner = ResolveOwner(ownerReference);
			if (!owner)
				return;

			if (binding)
			{
				*static_cast<ScriptComponentReference**>(
					generic->GetAddressOfReturnLocation()) =
						MakeReference(
							*owner,
							FindFirstComponent(
								*owner, *binding));
				return;
			}

			*static_cast<asIScriptObject**>(
				generic->GetAddressOfReturnLocation()) =
					FindFirstScriptComponent(
						*owner, *requestedType);
		}

		void HasComponentTemplate(asIScriptGeneric* generic)
		{
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			asITypeInfo* requestedType =
				GetTemplateType(*generic);
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			GameObject* owner =
				(binding ||
					IsScriptComponentType(requestedType))
					? ResolveOwner(ownerReference)
					: nullptr;
			bool result = false;
			if (owner && binding)
			{
				result =
					FindFirstComponent(*owner, *binding) !=
					nullptr;
			}
			else if (owner)
			{
				asIScriptObject* object =
					FindFirstScriptComponent(
						*owner, *requestedType);
				result = object != nullptr;
				if (object)
					object->Release();
			}
			*static_cast<bool*>(
				generic->GetAddressOfReturnLocation()) = result;
			if (!binding &&
				!IsScriptComponentType(requestedType))
			{
				SetScriptException(
					"The requested type is not an engine or script component.");
			}
		}

		void TryGetComponentTemplate(asIScriptGeneric* generic)
		{
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			asITypeInfo* requestedType =
				GetTemplateType(*generic);
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			GameObject* owner =
				(binding ||
					IsScriptComponentType(requestedType))
					? ResolveOwner(ownerReference)
					: nullptr;

			bool found = false;
			if (binding)
			{
				auto** output =
					static_cast<ScriptComponentReference**>(
						generic->GetArgAddress(0));
				if (output)
					*output = nullptr;
				Component* component = owner
					? FindFirstComponent(*owner, *binding)
					: nullptr;
				if (output && component)
					*output = MakeReference(*owner, component);
				found = component != nullptr;
			}
			else if (IsScriptComponentType(requestedType))
			{
				auto** output =
					static_cast<asIScriptObject**>(
						generic->GetArgAddress(0));
				if (output)
					*output = nullptr;
				asIScriptObject* object = owner
					? FindFirstScriptComponent(
						*owner, *requestedType)
					: nullptr;
				if (output)
					*output = object;
				else if (object)
					object->Release();
				found = object != nullptr;
			}

			*static_cast<bool*>(
				generic->GetAddressOfReturnLocation()) =
					found;
			if (!binding &&
				!IsScriptComponentType(requestedType))
			{
				SetScriptException(
					"The requested type is not an engine or script component.");
			}
		}

		void GetComponentsTemplate(asIScriptGeneric* generic)
		{
			*static_cast<CScriptArray**>(
				generic->GetAddressOfReturnLocation()) = nullptr;
			asITypeInfo* requestedType =
				GetTemplateType(*generic);
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			if (!binding &&
				!IsScriptComponentType(requestedType))
			{
				SetScriptException(
					"The requested type is not an engine or script component.");
				return;
			}

			asITypeInfo* arrayType =
				generic->GetEngine()->GetTypeInfoById(
					generic->GetFunction()->GetReturnTypeId());
			if (!arrayType)
			{
				SetScriptException(
					"The component array type could not be created.");
				return;
			}

			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			GameObject* owner = ResolveOwner(ownerReference);
			CScriptArray* result = CScriptArray::Create(arrayType);
			if (owner && binding)
			{
				for (Component* component : owner->components)
				{
					if (!component ||
						component->flag_for_removal ||
						component->GetType() != binding->engineType)
					{
						continue;
					}

					ScriptComponentReference* reference =
						MakeReference(*owner, component);
					result->InsertLast(&reference);
					reference->Release();
				}
			}
			else if (owner)
			{
				for (Component* component : owner->components)
				{
					if (!component ||
						component->flag_for_removal ||
						component->GetType() !=
							Component::Script)
					{
						continue;
					}

					auto* script =
						static_cast<ComponentScript*>(
							component);
					asIScriptObject* object =
						script->AcquireScriptObject(
							*requestedType);
					if (!object)
						continue;
					result->InsertLast(&object);
					object->Release();
				}
			}

			*static_cast<CScriptArray**>(
				generic->GetAddressOfReturnLocation()) = result;
		}

		void AddComponentTemplate(asIScriptGeneric* generic)
		{
			auto* ownerReference =
				static_cast<ScriptGameObjectReference*>(
					generic->GetObject());
			const ComponentBinding* binding =
				GetTemplateBinding(*generic);
			if (!binding || !binding->creatable)
			{
				SetScriptException(
					"The requested component type cannot be created.");
				return;
			}

			GameObject* owner = ResolveOwner(ownerReference);
			Component* component = owner
				? FindFirstComponent(*owner, *binding)
				: nullptr;
			const bool create =
				owner && (!component || binding->allowMultiple);
			if (create)
				component = owner->CreateComponent(binding->engineType);
			if (create && component && App && App->IsPlay())
			{
				component->OnStart();
				component->OnPlay();
			}

			*static_cast<ScriptComponentReference**>(
				generic->GetAddressOfReturnLocation()) =
					owner ? MakeReference(*owner, component) : nullptr;
		}

		void RemoveComponent(
			ScriptComponentReference* componentReference,
			const ScriptGameObjectReference* ownerReference)
		{
			GameObject* owner = ResolveOwner(ownerReference);
			Component* component =
				componentReference
					? componentReference->Resolve()
					: nullptr;
			if (!owner || !component)
				return;
			if (component->GetGameObject() != owner)
			{
				SetScriptException(
					"The component belongs to another GameObject.");
				return;
			}

			const ComponentBinding* binding = nullptr;
			for (const ComponentBinding& candidate : ComponentBindings)
			{
				if (candidate.engineType == component->GetType())
				{
					binding = &candidate;
					break;
				}
			}
			if (!binding || !binding->removable)
			{
				SetScriptException(
					"The component type cannot be removed from script.");
				return;
			}
			owner->RemoveComponent(component);
		}

		bool GetComponentEnabled(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			return component && component->IsActive();
		}

		void SetComponentEnabled(
			bool enabled,
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (component)
				component->SetActive(enabled);
		}

		std::string GetComponentTypeName(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			return component
				? component->GetTypeStr()
				: std::string();
		}

		ScriptGameObjectReference* GetComponentGameObject(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			GameObject* owner =
				component ? component->GetGameObject() : nullptr;
			return owner
				? MakeGameObjectReference(owner->GetUID())
				: nullptr;
		}

		bool ComponentsEqual(
			const ScriptComponentReference* other,
			const ScriptComponentReference* reference)
		{
			return other == reference ||
				(other && reference &&
					other->GetObjectId() ==
						reference->GetObjectId() &&
					other->GetComponentId() ==
						reference->GetComponentId());
		}

		ScriptComponentReference* CastRigidBodyToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToRigidBody(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::RigidBody)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastColliderToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToCollider(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::Collider)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastMeshRendererToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToMeshRenderer(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::MeshRenderer)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastCameraToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToCamera(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component || component->GetType() != Component::Camera)
				return nullptr;
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastAnimationToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToAnimation(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::Animation)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastAudioSourceToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToAudioSource(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::AudioSource)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastAudioListenerToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToAudioListener(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::AudioListener)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastReziAudioEmitterToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToReziAudioEmitter(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::ReziAudioEmitter)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastReziAudioListenerToComponent(
			ScriptComponentReference* reference)
		{
			if (reference)
				reference->AddRef();
			return reference;
		}

		ScriptComponentReference* CastComponentToReziAudioListener(
			ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::ReziAudioListener)
			{
				return nullptr;
			}
			reference->AddRef();
			return reference;
		}

		ComponentRigidBody* ResolveRigidBody(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::RigidBody)
			{
				SetScriptException(
					"The RigidBody reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentRigidBody*>(component);
		}

		ComponentCollider* ResolveCollider(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::Collider)
			{
				SetScriptException(
					"The Collider reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentCollider*>(component);
		}

		ComponentMeshRenderer* ResolveMeshRenderer(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::MeshRenderer)
			{
				SetScriptException(
					"The MeshRenderer reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentMeshRenderer*>(component);
		}

		ComponentCamera* ResolveCamera(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component || component->GetType() != Component::Camera)
			{
				SetScriptException(
					"The Camera reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentCamera*>(component);
		}

		ComponentAudioSource* ResolveAudioSource(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::AudioSource)
			{
				SetScriptException(
					"The AudioSource reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentAudioSource*>(component);
		}

		ComponentReziAudioEmitter* ResolveReziAudioEmitter(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::ReziAudioEmitter)
			{
				SetScriptException(
					"The ReziAudioEmitter reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentReziAudioEmitter*>(component);
		}

		ComponentAnimation* ResolveAnimation(
			const ScriptComponentReference* reference)
		{
			Component* component =
				reference ? reference->Resolve() : nullptr;
			if (!component ||
				component->GetType() != Component::Animation)
			{
				SetScriptException(
					"The Animation reference is no longer valid.");
				return nullptr;
			}
			return static_cast<ComponentAnimation*>(component);
		}

		ScriptResourceReference* GetAnimationStateMachine(
			const ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation
				? GetScriptResourceReference(
					animation->GetResourceUID(),
					Resource::state_machine)
				: nullptr;
		}

		void SetAnimationStateMachine(
			ScriptResourceReference* stateMachine,
			ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			if (!animation)
				return;
			const UID id = stateMachine
				? ResolveScriptResourceId(
					stateMachine, Resource::state_machine)
				: 0;
			if (!stateMachine || id != 0)
				animation->SetResource(id);
		}

		std::string GetAnimationCurrentState(
			const ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			const HashString state =
				animation
					? animation->GetActiveNode()
					: HashString();
			return state ? state.C_str() : std::string();
		}

		bool GetAnimationIsPlaying(
			const ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation && animation->IsPlaying();
		}

		float GetAnimationSpeed(
			const ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation ? animation->GetSpeed() : 0.0f;
		}

		void SetAnimationSpeed(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentAnimation* animation =
					ResolveAnimation(reference))
			{
				animation->SetSpeed(value);
			}
		}

		bool GetAnimationDebugDraw(
			const ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation && animation->GetDebugDraw();
		}

		void SetAnimationDebugDraw(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentAnimation* animation =
					ResolveAnimation(reference))
			{
				animation->SetDebugDraw(value);
			}
		}

		bool PlayAnimation(
			ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation && animation->PlayDefault();
		}

		bool PlayAnimationState(
			const std::string& state,
			std::uint32_t blendMilliseconds,
			ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation &&
				animation->PlayState(
					HashString(state.c_str()),
					blendMilliseconds);
		}

		bool SendAnimationTrigger(
			const std::string& trigger,
			ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation &&
				animation->SendTrigger(
					HashString(trigger.c_str()));
		}

		bool ResetAnimation(
			ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation && animation->ResetState();
		}

		void StopAnimation(
			ScriptComponentReference* reference)
		{
			if (ComponentAnimation* animation =
					ResolveAnimation(reference))
			{
				animation->StopPlayback();
			}
		}

		bool IsAnimationInState(
			const std::string& state,
			const ScriptComponentReference* reference)
		{
			ComponentAnimation* animation =
				ResolveAnimation(reference);
			return animation &&
				animation->IsInState(
					HashString(state.c_str()));
		}

		ScriptResourceReference* GetAudioSourceClip(
			const ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source
				? GetScriptResourceReference(
					source->GetResourceUID(), Resource::audio)
				: nullptr;
		}

		void SetAudioSourceClip(
			ScriptResourceReference* clip,
			ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			if (!source)
				return;

			const UID id = clip
				? ResolveScriptResourceId(clip, Resource::audio)
				: 0;
			if (!clip || id != 0)
				source->SetResource(id);
		}

		bool GetAudioSourceIs2D(
			const ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source && source->is_2d;
		}

		void SetAudioSourceIs2D(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentAudioSource* source =
					ResolveAudioSource(reference))
			{
				source->is_2d = value;
			}
		}

		bool GetAudioSourceIsPlaying(
			const ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source && source->IsPlaying();
		}

		bool GetAudioSourceIsPaused(
			const ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source && source->IsPaused();
		}

		bool PlayAudioSource(
			ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source && source->Play();
		}

		bool PauseAudioSource(
			ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source && source->Pause();
		}

		bool ResumeAudioSource(
			ScriptComponentReference* reference)
		{
			ComponentAudioSource* source =
				ResolveAudioSource(reference);
			return source && source->UnPause();
		}

		void StopAudioSource(
			ScriptComponentReference* reference)
		{
			if (ComponentAudioSource* source =
					ResolveAudioSource(reference))
			{
				source->Stop();
			}
		}

		ScriptResourceReference* GetReziAudioEmitterClip(
			const ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter
				? GetScriptResourceReference(
					emitter->GetClip(), Resource::audio)
				: nullptr;
		}

		void SetReziAudioEmitterClip(
			ScriptResourceReference* clip,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			if (!emitter)
				return;
			const UID id = clip
				? ResolveScriptResourceId(clip, Resource::audio)
				: 0;
			if (!clip || id != 0)
				emitter->SetClip(id);
		}

		float GetReziAudioEmitterVolume(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter ? emitter->settings.volume : 0.0f;
		}

		void SetReziAudioEmitterVolume(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->settings.volume = std::max(value, 0.0f);
			}
		}

		float GetReziAudioEmitterPitch(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter ? emitter->settings.pitch : 0.0f;
		}

		void SetReziAudioEmitterPitch(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->settings.pitch = std::max(value, 0.01f);
			}
		}

		bool GetReziAudioEmitterSpatial(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->settings.spatial.enabled;
		}

		void SetReziAudioEmitterSpatial(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->settings.spatial.enabled = value;
			}
		}

		void SetReziAudioFloatParameter(
			const std::string& name,
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->SetRuntimeParameter(name, value);
			}
		}

		float GetReziAudioFloatParameter(
			const std::string& name,
			float fallback,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const float* result = value
				? std::get_if<float>(value)
				: nullptr;
			return result ? *result : fallback;
		}

		void SetReziAudioIntParameter(
			const std::string& name,
			int value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->SetRuntimeParameter(name, value);
			}
		}

		int GetReziAudioIntParameter(
			const std::string& name,
			int fallback,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const int* result = value
				? std::get_if<int>(value)
				: nullptr;
			return result ? *result : fallback;
		}

		void SetReziAudioBoolParameter(
			const std::string& name,
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->SetRuntimeParameter(name, value);
			}
		}

		bool GetReziAudioBoolParameter(
			const std::string& name,
			bool fallback,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const bool* result = value
				? std::get_if<bool>(value)
				: nullptr;
			return result ? *result : fallback;
		}

		void SetReziAudioVectorParameter(
			const std::string& name,
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->SetRuntimeParameter(
					name, float3(value.x, value.y, value.z));
			}
		}

		ScriptVector3 GetReziAudioVectorParameter(
			const std::string& name,
			const ScriptVector3& fallback,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const float3* result = value
				? std::get_if<float3>(value)
				: nullptr;
			return result
				? ScriptVector3{result->x, result->y, result->z}
				: fallback;
		}

		void SetReziAudioVector2Parameter(
			const std::string& name,
			const ScriptVector2& value,
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->SetRuntimeParameter(
					name, float2(value.x, value.y));
			}
		}

		ScriptVector2 GetReziAudioVector2Parameter(
			const std::string& name,
			const ScriptVector2& fallback,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const float2* result = value
				? std::get_if<float2>(value)
				: nullptr;
			return result
				? ScriptVector2{result->x, result->y}
				: fallback;
		}

		template <typename Value>
		CScriptArray* CreateReziAudioValueArray(
			const char* declaration,
			const std::vector<Value>* values)
		{
			asIScriptContext* context = asGetActiveContext();
			asITypeInfo* arrayType =
				context
					? context->GetEngine()->GetTypeInfoByDecl(declaration)
					: nullptr;
			if (!arrayType)
			{
				SetScriptException(
					"The requested ReziAudio parameter array "
					"type could not be created.");
				return nullptr;
			}

			CScriptArray* result = CScriptArray::Create(arrayType);
			if (!values)
				return result;

			result->Resize(static_cast<asUINT>(values->size()));
			for (asUINT index = 0; index < result->GetSize(); ++index)
			{
				*static_cast<Value*>(result->At(index)) =
					(*values)[index];
			}
			return result;
		}

		void SetReziAudioFloatArrayParameter(
			const std::string& name,
			const CScriptArray* values,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			if (!emitter)
				return;

			EGE::ReziAudio::FloatArray converted;
			if (values)
			{
				converted.reserve(values->GetSize());
				for (asUINT index = 0; index < values->GetSize(); ++index)
				{
					converted.push_back(
						*static_cast<const float*>(values->At(index)));
				}
			}
			emitter->SetRuntimeParameter(name, std::move(converted));
		}

		CScriptArray* GetReziAudioFloatArrayParameter(
			const std::string& name,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const auto* values = value
				? std::get_if<EGE::ReziAudio::FloatArray>(value)
				: nullptr;
			return CreateReziAudioValueArray(
				"array<float>", values);
		}

		void SetReziAudioIntegerArrayParameter(
			const std::string& name,
			const CScriptArray* values,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			if (!emitter)
				return;

			EGE::ReziAudio::IntegerArray converted;
			if (values)
			{
				converted.reserve(values->GetSize());
				for (asUINT index = 0; index < values->GetSize(); ++index)
				{
					converted.push_back(
						*static_cast<const int*>(values->At(index)));
				}
			}
			emitter->SetRuntimeParameter(name, std::move(converted));
		}

		CScriptArray* GetReziAudioIntegerArrayParameter(
			const std::string& name,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const auto* values = value
				? std::get_if<EGE::ReziAudio::IntegerArray>(value)
				: nullptr;
			return CreateReziAudioValueArray(
				"array<int>", values);
		}

		void SetReziAudioClipParameter(
			const std::string& name,
			ScriptResourceReference* clip,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			if (!emitter)
				return;
			const UID id = clip
				? ResolveScriptResourceId(clip, Resource::audio)
				: 0;
			if (!clip || id != 0)
				emitter->SetRuntimeAudioClipParameter(name, id);
		}

		ScriptResourceReference* GetReziAudioClipParameter(
			const std::string& name,
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const auto* clip = value
				? std::get_if<EGE::ReziAudio::AudioClipReference>(value)
				: nullptr;
			return clip && clip->assetId != 0
				? GetScriptResourceReference(
					static_cast<UID>(clip->assetId),
					Resource::audio)
				: nullptr;
		}

		void SetReziAudioClipArrayParameter(
			const std::string& name,
			const CScriptArray* clips,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			if (!emitter)
				return;

			EGE::ReziAudio::AudioClipArray converted;
			if (clips)
			{
				converted.reserve(clips->GetSize());
				for (asUINT index = 0; index < clips->GetSize(); ++index)
				{
					auto* clip =
						*static_cast<ScriptResourceReference* const*>(
							clips->At(index));
					const UID id = clip
						? ResolveScriptResourceId(
							clip, Resource::audio)
						: 0;
					if (clip && id == 0)
						return;
					converted.push_back({
						static_cast<std::uint64_t>(id), {}});
				}
			}
			emitter->SetRuntimeParameter(name, std::move(converted));
		}

		CScriptArray* GetReziAudioClipArrayParameter(
			const std::string& name,
			const ScriptComponentReference* reference)
		{
			asIScriptContext* context = asGetActiveContext();
			asITypeInfo* arrayType =
				context
					? context->GetEngine()->GetTypeInfoByDecl(
						"array<AudioClip@>")
					: nullptr;
			if (!arrayType)
			{
				SetScriptException(
					"The AudioClip parameter array type could "
					"not be created.");
				return nullptr;
			}

			CScriptArray* result = CScriptArray::Create(arrayType);
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			const auto* value = emitter
				? emitter->GetRuntimeParameter(name)
				: nullptr;
			const auto* clips = value
				? std::get_if<EGE::ReziAudio::AudioClipArray>(value)
				: nullptr;
			if (!clips)
				return result;

			for (const auto& clip : *clips)
			{
				ScriptResourceReference* resource =
					clip.assetId != 0
						? GetScriptResourceReference(
							static_cast<UID>(clip.assetId),
							Resource::audio)
						: nullptr;
				result->InsertLast(&resource);
				if (resource)
					resource->Release();
			}
			return result;
		}

		void ClearReziAudioParameters(
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->ClearRuntimeParameters();
			}
		}

		bool PlayReziAudioEmitter(
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->Play();
		}

		bool PlayReziAudioEmitterWithFade(
			float durationSeconds,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter &&
				emitter->PlayWithFade(durationSeconds);
		}

		bool PauseReziAudioEmitter(
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->Pause();
		}

		bool ResumeReziAudioEmitter(
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->Resume();
		}

		void StopReziAudioEmitter(
			ScriptComponentReference* reference)
		{
			if (ComponentReziAudioEmitter* emitter =
					ResolveReziAudioEmitter(reference))
			{
				emitter->Stop();
			}
		}

		bool StopReziAudioEmitterWithFade(
			float durationSeconds,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter &&
				emitter->StopWithFade(durationSeconds);
		}

		bool FadeReziAudioEmitterTo(
			float targetVolume,
			float durationSeconds,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter &&
				emitter->FadeTo(targetVolume, durationSeconds);
		}

		bool SeekReziAudioEmitter(
			float seconds,
			ScriptComponentReference* reference)
		{
			ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->Seek(seconds);
		}

		float GetReziAudioPlaybackSeconds(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter ? emitter->GetPlaybackSeconds() : 0.0f;
		}

		float GetReziAudioPlaybackLengthSeconds(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter
				? emitter->GetPlaybackLengthSeconds()
				: 0.0f;
		}

		float GetReziAudioPlaybackPercentage(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter
				? emitter->GetPlaybackPercentage()
				: 0.0f;
		}

		bool IsReziAudioEmitterPlaying(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->IsPlaying();
		}

		bool IsReziAudioEmitterPaused(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->IsPaused();
		}

		bool IsReziAudioEmitterFinished(
			const ScriptComponentReference* reference)
		{
			const ComponentReziAudioEmitter* emitter =
				ResolveReziAudioEmitter(reference);
			return emitter && emitter->IsFinished();
		}

		float GetCameraFieldOfView(
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			return camera ? camera->GetFOV() : 0.0f;
		}

		void SetCameraFieldOfView(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentCamera* camera = ResolveCamera(reference))
				camera->SetFOV(std::clamp(value, 1.0f, 179.0f));
		}

		float GetCameraNearClip(
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			return camera ? camera->GetNearPlaneDist() : 0.0f;
		}

		void SetCameraNearClip(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentCamera* camera = ResolveCamera(reference))
				camera->SetNearPlaneDist(value);
		}

		float GetCameraFarClip(
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			return camera ? camera->GetFarPlaneDist() : 0.0f;
		}

		void SetCameraFarClip(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentCamera* camera = ResolveCamera(reference))
				camera->SetFarPlaneDist(value);
		}

		float GetCameraAspect(
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			return camera ? camera->GetAspectRatio() : 0.0f;
		}

		void SetCameraAspect(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentCamera* camera = ResolveCamera(reference))
			{
				if (value > 0.0f)
					camera->SetAspectRatio(value);
			}
		}

		bool GetCameraIsMain(
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			return camera && App && App->renderer3D &&
				App->renderer3D->active_camera == camera;
		}

		void RefreshCameraTransform(ComponentCamera& camera)
		{
			camera.OnUpdateTransform();
		}

		bool GetCameraTargetSize(float& width, float& height)
		{
			if (App && App->renderer3D &&
				App->renderer3D->viewport &&
				App->renderer3D->viewport->GetScene())
			{
				const SceneViewport* scene =
					App->renderer3D->viewport->GetScene();
				if (scene->GetWidth() > 0 && scene->GetHeight() > 0)
				{
					width = static_cast<float>(scene->GetWidth());
					height = static_cast<float>(scene->GetHeight());
					return true;
				}
			}
			if (App && App->window &&
				App->window->GetWidth() > 0 &&
				App->window->GetHeight() > 0)
			{
				width = static_cast<float>(App->window->GetWidth());
				height = static_cast<float>(App->window->GetHeight());
				return true;
			}
			return false;
		}

		ScriptVector3 CameraWorldToViewportPoint(
			const ScriptVector3& worldPoint,
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			if (!camera)
				return {};
			RefreshCameraTransform(*camera);
			const float3 world{
				worldPoint.x, worldPoint.y, worldPoint.z};
			const float3 projected = camera->frustum.Project(world);
			return {
				projected.x * 0.5f + 0.5f,
				projected.y * 0.5f + 0.5f,
				(world - camera->frustum.pos).Dot(
					camera->frustum.front)};
		}

		ScriptVector3 CameraViewportToWorldPoint(
			const ScriptVector3& viewportPoint,
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			if (!camera)
				return {};
			RefreshCameraTransform(*camera);
			const Ray ray = camera->frustum.UnProject(
				viewportPoint.x * 2.0f - 1.0f,
				viewportPoint.y * 2.0f - 1.0f);
			const float3 result =
				ray.pos + ray.dir * viewportPoint.z;
			return {result.x, result.y, result.z};
		}

		ScriptVector3 CameraWorldToScreenPoint(
			const ScriptVector3& worldPoint,
			const ScriptComponentReference* reference)
		{
			ScriptVector3 viewport =
				CameraWorldToViewportPoint(worldPoint, reference);
			float width = 0.0f;
			float height = 0.0f;
			if (!GetCameraTargetSize(width, height))
				return viewport;
			viewport.x *= width;
			viewport.y = (1.0f - viewport.y) * height;
			return viewport;
		}

		ScriptVector3 CameraScreenToWorldPoint(
			const ScriptVector3& screenPoint,
			const ScriptComponentReference* reference)
		{
			float width = 0.0f;
			float height = 0.0f;
			if (!GetCameraTargetSize(width, height))
				return {};
			return CameraViewportToWorldPoint(
				{
					screenPoint.x / width,
					1.0f - screenPoint.y / height,
					screenPoint.z},
				reference);
		}

		ScriptVector3 CameraScreenPointToDirection(
			const ScriptVector2& screenPoint,
			const ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			float width = 0.0f;
			float height = 0.0f;
			if (!camera || !GetCameraTargetSize(width, height))
				return {};
			RefreshCameraTransform(*camera);
			const float x =
				screenPoint.x / width * 2.0f - 1.0f;
			const float y =
				1.0f - screenPoint.y / height * 2.0f;
			const float3 direction =
				camera->frustum.UnProject(x, y).dir.Normalized();
			return {direction.x, direction.y, direction.z};
		}

		void CameraLookAt(
			const ScriptVector3& worldPoint,
			ScriptComponentReference* reference)
		{
			ComponentCamera* camera = ResolveCamera(reference);
			if (!camera)
				return;
			RefreshCameraTransform(*camera);
			camera->Look({
				worldPoint.x, worldPoint.y, worldPoint.z});
			camera->OnUpdateFrustum();
		}

		ScriptComponentReference* GetMainCamera()
		{
			if (!App || !App->renderer3D)
				return nullptr;
			ComponentCamera* camera = App->renderer3D->active_camera;
			GameObject* owner = camera ? camera->GetGameObject() : nullptr;
			return owner ? MakeReference(*owner, camera) : nullptr;
		}

		ScriptResourceReference* GetMeshResource(std::uint64_t id)
		{
			return GetScriptResourceReference(id, Resource::mesh);
		}

		ScriptResourceReference* GetMaterialResource(
			std::uint64_t id)
		{
			return GetScriptResourceReference(id, Resource::material);
		}

		UID ResolveResourceId(
			const ScriptResourceReference* reference,
			Resource::Type expectedType)
		{
			if (!reference)
				return 0;
			return ResolveScriptResourceId(reference, expectedType);
		}

		ScriptResourceReference* GetRendererMesh(
			const ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			return renderer
				? GetMeshResource(renderer->GetMeshUID())
				: nullptr;
		}

		void SetRendererMesh(
			ScriptResourceReference* mesh,
			ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			if (!renderer)
				return;
			const UID id =
				mesh
					? ResolveResourceId(mesh, Resource::mesh)
					: 0;
			if (!mesh || id != 0)
				renderer->SetMeshRes(id);
		}

		std::uint32_t GetRendererMaterialCount(
			const ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			return renderer
				? static_cast<std::uint32_t>(
					renderer->GetMaterialCount())
				: 0;
		}

		ScriptResourceReference* GetRendererMaterial(
			std::uint32_t index,
			const ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			return renderer
				? GetMaterialResource(
					renderer->GetMaterialUID(index))
				: nullptr;
		}

		void SetRendererMaterial(
			std::uint32_t index,
			ScriptResourceReference* material,
			ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			if (!renderer)
				return;
			const UID id =
				material
					? ResolveResourceId(
						material, Resource::material)
					: 0;
			if (!material || id != 0)
				renderer->SetMaterialRes(index, id);
		}

		ScriptResourceReference* GetRendererPrimaryMaterial(
			const ScriptComponentReference* reference)
		{
			return GetRendererMaterial(0, reference);
		}

		void SetRendererPrimaryMaterial(
			ScriptResourceReference* material,
			ScriptComponentReference* reference)
		{
			SetRendererMaterial(0, material, reference);
		}

		void AddRendererMaterial(
			ScriptResourceReference* material,
			ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			if (!renderer)
				return;
			const UID id =
				material
					? ResolveResourceId(
						material, Resource::material)
					: 0;
			if (!material || id != 0)
				renderer->AddMaterialRes(id);
		}

		void RemoveRendererMaterial(
			std::uint32_t index,
			ScriptComponentReference* reference)
		{
			if (ComponentMeshRenderer* renderer =
					ResolveMeshRenderer(reference))
			{
				renderer->RemoveMaterialRes(index);
			}
		}

		void ClearRendererMaterials(
			ScriptComponentReference* reference)
		{
			if (ComponentMeshRenderer* renderer =
					ResolveMeshRenderer(reference))
			{
				renderer->ClearMaterialResources();
			}
		}

		void SetRendererMaterials(
			const CScriptArray* materials,
			ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			if (!renderer)
				return;
			if (!materials || materials->GetSize() == 0)
			{
				renderer->ClearMaterialResources();
				return;
			}

			std::vector<UID> resourceIds;
			resourceIds.reserve(materials->GetSize());
			for (asUINT index = 0;
				index < materials->GetSize();
				++index)
			{
				auto* material =
					*static_cast<ScriptResourceReference* const*>(
						materials->At(index));
				const UID id = material
					? ResolveResourceId(
						material, Resource::material)
					: 0;
				if (material && id == 0)
					return;
				resourceIds.push_back(id);
			}

			renderer->ClearMaterialResources();
			renderer->SetMaterialCount(resourceIds.size());
			for (std::size_t index = 0;
				index < resourceIds.size();
				++index)
			{
				renderer->SetMaterialRes(
					index, resourceIds[index]);
			}
		}

		CScriptArray* GetRendererMaterials(
			const ScriptComponentReference* reference)
		{
			asIScriptContext* context = asGetActiveContext();
			asITypeInfo* arrayType =
				context
					? context->GetEngine()->GetTypeInfoByDecl(
						"array<Material@>")
					: nullptr;
			if (!arrayType)
			{
				SetScriptException(
					"The Material array type could not be created.");
				return nullptr;
			}

			CScriptArray* result = CScriptArray::Create(arrayType);
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			if (!renderer)
				return result;

			for (UID id : renderer->GetMaterialUIDs())
			{
				ScriptResourceReference* material =
					GetMaterialResource(id);
				result->InsertLast(&material);
				if (material)
					material->Release();
			}
			return result;
		}

		bool GetRendererVisible(
			const ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			return renderer && renderer->GetVisible();
		}

		void SetRendererVisible(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentMeshRenderer* renderer =
					ResolveMeshRenderer(reference))
			{
				renderer->SetVisible(value);
			}
		}

		bool GetRendererCastShadows(
			const ScriptComponentReference* reference)
		{
			ComponentMeshRenderer* renderer =
				ResolveMeshRenderer(reference);
			return renderer && renderer->CastShadows();
		}

		void SetRendererCastShadows(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentMeshRenderer* renderer =
					ResolveMeshRenderer(reference))
			{
				renderer->SetCastShadows(value);
			}
		}

		ScriptVector3 ToScriptVector(const float3& value)
		{
			return {value.x, value.y, value.z};
		}

		float3 ToEngineVector(const ScriptVector3& value)
		{
			return {value.x, value.y, value.z};
		}

		int GetColliderShape(
			const ScriptComponentReference* reference)
		{
			ComponentCollider* collider = ResolveCollider(reference);
			return collider
				? static_cast<int>(collider->GetShapeType())
				: 0;
		}

		void SetColliderShape(
			int value,
			ScriptComponentReference* reference)
		{
			if (ComponentCollider* collider = ResolveCollider(reference))
			{
				collider->SetShapeType(
					static_cast<ComponentCollider::ShapeType>(value));
			}
		}

		bool GetColliderIsTrigger(
			const ScriptComponentReference* reference)
		{
			ComponentCollider* collider = ResolveCollider(reference);
			return collider && collider->IsTrigger();
		}

		void SetColliderIsTrigger(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentCollider* collider = ResolveCollider(reference))
				collider->SetTrigger(value);
		}

#define EGE_COLLIDER_VECTOR_PROPERTY(Name) \
		ScriptVector3 GetCollider##Name( \
			const ScriptComponentReference* reference) \
		{ \
			ComponentCollider* collider = ResolveCollider(reference); \
			return collider \
				? ToScriptVector(collider->Get##Name()) \
				: ScriptVector3{}; \
		} \
		void SetCollider##Name( \
			const ScriptVector3& value, \
			ScriptComponentReference* reference) \
		{ \
			if (ComponentCollider* collider = ResolveCollider(reference)) \
				collider->Set##Name(ToEngineVector(value)); \
		}

		EGE_COLLIDER_VECTOR_PROPERTY(SphereCenter)
		EGE_COLLIDER_VECTOR_PROPERTY(BoxCenter)
		EGE_COLLIDER_VECTOR_PROPERTY(BoxHalfExtents)
		EGE_COLLIDER_VECTOR_PROPERTY(BoxRotation)
		EGE_COLLIDER_VECTOR_PROPERTY(CapsuleStart)
		EGE_COLLIDER_VECTOR_PROPERTY(CapsuleEnd)

#undef EGE_COLLIDER_VECTOR_PROPERTY

		float GetColliderSphereRadius(
			const ScriptComponentReference* reference)
		{
			ComponentCollider* collider = ResolveCollider(reference);
			return collider ? collider->GetSphereRadius() : 0.0f;
		}

		void SetColliderSphereRadius(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentCollider* collider = ResolveCollider(reference))
				collider->SetSphereRadius(value);
		}

		float GetColliderCapsuleRadius(
			const ScriptComponentReference* reference)
		{
			ComponentCollider* collider = ResolveCollider(reference);
			return collider ? collider->GetCapsuleRadius() : 0.0f;
		}

		void SetColliderCapsuleRadius(
			float value,
			ScriptComponentReference* reference)
		{
			if (ComponentCollider* collider = ResolveCollider(reference))
				collider->SetCapsuleRadius(value);
		}

		int GetRigidBodyType(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body ? static_cast<int>(body->GetBodyType()) : 0;
		}

		void SetRigidBodyType(
			int value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->SetBodyType(
					static_cast<ComponentRigidBody::BodyType>(value));
			}
		}

		int GetRigidBodyBehaviour(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body
				? static_cast<int>(body->GetBehaviour())
				: 0;
		}

		void SetRigidBodyBehaviour(
			int value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->SetBehaviour(
					static_cast<ComponentRigidBody::BodyBehaviour>(
						value));
			}
		}

#define EGE_RIGID_FLOAT_PROPERTY(Name) \
		float Get##Name(const ScriptComponentReference* reference) \
		{ \
			ComponentRigidBody* body = ResolveRigidBody(reference); \
			return body ? body->Get##Name() : 0.0f; \
		} \
		void Set##Name(float value, ScriptComponentReference* reference) \
		{ \
			if (ComponentRigidBody* body = ResolveRigidBody(reference)) \
				body->Set##Name(value); \
		}

		EGE_RIGID_FLOAT_PROPERTY(Mass)
		EGE_RIGID_FLOAT_PROPERTY(Restitution)
		EGE_RIGID_FLOAT_PROPERTY(Friction)
		EGE_RIGID_FLOAT_PROPERTY(RollingFriction)
		EGE_RIGID_FLOAT_PROPERTY(LinearDamping)
		EGE_RIGID_FLOAT_PROPERTY(AngularDamping)
		EGE_RIGID_FLOAT_PROPERTY(SphereRadius)
		EGE_RIGID_FLOAT_PROPERTY(CapsuleRadius)

#undef EGE_RIGID_FLOAT_PROPERTY

#define EGE_RIGID_VECTOR_PROPERTY(Name) \
		ScriptVector3 Get##Name( \
			const ScriptComponentReference* reference) \
		{ \
			ComponentRigidBody* body = ResolveRigidBody(reference); \
			return body \
				? ToScriptVector(body->Get##Name()) \
				: ScriptVector3{}; \
		} \
		void Set##Name( \
			const ScriptVector3& value, \
			ScriptComponentReference* reference) \
		{ \
			if (ComponentRigidBody* body = ResolveRigidBody(reference)) \
				body->Set##Name(ToEngineVector(value)); \
		}

		EGE_RIGID_VECTOR_PROPERTY(LinearFactor)
		EGE_RIGID_VECTOR_PROPERTY(AngularFactor)
		EGE_RIGID_VECTOR_PROPERTY(SphereCenter)
		EGE_RIGID_VECTOR_PROPERTY(BoxCenter)
		EGE_RIGID_VECTOR_PROPERTY(BoxHalfExtents)
		EGE_RIGID_VECTOR_PROPERTY(BoxRotation)
		EGE_RIGID_VECTOR_PROPERTY(CapsuleStart)
		EGE_RIGID_VECTOR_PROPERTY(CapsuleEnd)
		EGE_RIGID_VECTOR_PROPERTY(LinearVelocity)
		EGE_RIGID_VECTOR_PROPERTY(AngularVelocity)
		EGE_RIGID_VECTOR_PROPERTY(Gravity)

#undef EGE_RIGID_VECTOR_PROPERTY

#define EGE_RIGID_READONLY_VECTOR(Name) \
		ScriptVector3 Get##Name( \
			const ScriptComponentReference* reference) \
		{ \
			ComponentRigidBody* body = ResolveRigidBody(reference); \
			return body \
				? ToScriptVector(body->Get##Name()) \
				: ScriptVector3{}; \
		}

		EGE_RIGID_READONLY_VECTOR(CenterOfMass)
		EGE_RIGID_READONLY_VECTOR(TotalForce)
		EGE_RIGID_READONLY_VECTOR(TotalTorque)

#undef EGE_RIGID_READONLY_VECTOR

		bool GetHasRuntimeBody(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body && body->HasRuntimeBody();
		}

		bool GetIsAwake(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body && body->IsAwake();
		}

		bool GetUseWorldGravity(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body && body->GetUseWorldGravity();
		}

		void SetUseWorldGravity(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->SetUseWorldGravity(value);
		}

		bool GetIsTrigger(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body && body->IsTrigger();
		}

		void SetIsTrigger(
			bool value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->SetTrigger(value);
		}

		unsigned int GetCollisionLayer(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body ? body->GetCollisionLayer() : 0;
		}

		void SetCollisionLayer(
			unsigned int value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->SetCollisionLayer(value);
		}

		unsigned int GetCollisionMask(
			const ScriptComponentReference* reference)
		{
			ComponentRigidBody* body = ResolveRigidBody(reference);
			return body ? body->GetCollisionMask() : 0;
		}

		void SetCollisionMask(
			unsigned int value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->SetCollisionMask(value);
		}

		void WakeUp(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->WakeUp();
		}

		void Sleep(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->Sleep();
		}

		void ClearForces(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ClearForces();
		}

		void ApplyCentralForce(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyCentralForce(ToEngineVector(value));
		}

		void ApplyForce(
			const ScriptVector3& force,
			const ScriptVector3& relativePosition,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->ApplyForce(
					ToEngineVector(force),
					ToEngineVector(relativePosition));
			}
		}

		void ApplyCentralImpulse(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyCentralImpulse(ToEngineVector(value));
		}

		void ApplyImpulse(
			const ScriptVector3& impulse,
			const ScriptVector3& relativePosition,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
			{
				body->ApplyImpulse(
					ToEngineVector(impulse),
					ToEngineVector(relativePosition));
			}
		}

		void ApplyTorque(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyTorque(ToEngineVector(value));
		}

		void ApplyTorqueImpulse(
			const ScriptVector3& value,
			ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->ApplyTorqueImpulse(ToEngineVector(value));
		}

		void RebuildBody(ScriptComponentReference* reference)
		{
			if (ComponentRigidBody* body = ResolveRigidBody(reference))
				body->RebuildBody();
		}

		bool RegisterComponentHandle(
			asIScriptEngine& engine,
			const char* type,
			std::string& error)
		{
			const bool registered =
				engine.RegisterObjectType(type, 0, asOBJ_REF) >= 0 &&
				engine.RegisterObjectBehaviour(
					type, asBEHAVE_ADDREF, "void f()",
					asMETHOD(ScriptComponentReference, AddRef),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectBehaviour(
					type, asBEHAVE_RELEASE, "void f()",
					asMETHOD(ScriptComponentReference, Release),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "uint get_id() const property",
					asMETHOD(
						ScriptComponentReference,
						GetComponentId),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool get_valid() const property",
					asMETHOD(ScriptComponentReference, IsValid),
					asCALL_THISCALL) >= 0 &&
				engine.RegisterObjectMethod(
					type, "bool get_enabled() const property",
					asFUNCTION(GetComponentEnabled),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "void set_enabled(bool) property",
					asFUNCTION(SetComponentEnabled),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "string get_typeName() const property",
					asFUNCTION(GetComponentTypeName),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type, "GameObject@ get_gameObject() const property",
					asFUNCTION(GetComponentGameObject),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					type,
					("bool opEquals(const " +
					 std::string(type) +
					 "@+ other) const").c_str(),
					asFUNCTION(ComponentsEqual),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error =
				"Could not register the '" +
				std::string(type) + "' component handle.";
			return false;
		}

		bool RegisterMeshRendererApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(
					engine, "MeshRenderer", error))
			{
				return false;
			}

			const bool registered =
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"Component@+ opImplCast()",
					asFUNCTION(CastMeshRendererToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component",
					"MeshRenderer@+ opCast()",
					asFUNCTION(CastComponentToMeshRenderer),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"Mesh@ get_mesh() const property",
					asFUNCTION(GetRendererMesh),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void set_mesh(Mesh@+) property",
					asFUNCTION(SetRendererMesh),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"Mesh@ GetMesh() const",
					asFUNCTION(GetRendererMesh),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void SetMesh(Mesh@+ mesh)",
					asFUNCTION(SetRendererMesh),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"Material@ get_material() const property",
					asFUNCTION(GetRendererPrimaryMaterial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void set_material(Material@+) property",
					asFUNCTION(SetRendererPrimaryMaterial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"uint get_materialCount() const property",
					asFUNCTION(GetRendererMaterialCount),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"Material@ GetMaterial(uint index) const",
					asFUNCTION(GetRendererMaterial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"array<Material@>@ GetMaterials() const",
					asFUNCTION(GetRendererMaterials),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"array<Material@>@ get_materials() const property",
					asFUNCTION(GetRendererMaterials),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void set_materials(array<Material@>@+) property",
					asFUNCTION(SetRendererMaterials),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void SetMaterials(array<Material@>@+ materials)",
					asFUNCTION(SetRendererMaterials),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void SetMaterial("
						"uint index, Material@+ material)",
					asFUNCTION(SetRendererMaterial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void AddMaterial(Material@+ material)",
					asFUNCTION(AddRendererMaterial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void RemoveMaterialAt(uint index)",
					asFUNCTION(RemoveRendererMaterial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void ClearMaterials()",
					asFUNCTION(ClearRendererMaterials),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"bool get_visible() const property",
					asFUNCTION(GetRendererVisible),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void set_visible(bool) property",
					asFUNCTION(SetRendererVisible),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"bool get_castShadows() const property",
					asFUNCTION(GetRendererCastShadows),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"MeshRenderer",
					"void set_castShadows(bool) property",
					asFUNCTION(SetRendererCastShadows),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error = "Could not register the MeshRenderer API.";
			return false;
		}

		bool RegisterCameraApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(engine, "Camera", error))
				return false;

			const bool registered =
				engine.RegisterObjectMethod(
					"Camera", "Component@+ opImplCast()",
					asFUNCTION(CastCameraToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "Camera@+ opCast()",
					asFUNCTION(CastComponentToCamera),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "float get_fieldOfView() const property",
					asFUNCTION(GetCameraFieldOfView),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "void set_fieldOfView(float) property",
					asFUNCTION(SetCameraFieldOfView),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "float get_nearClipPlane() const property",
					asFUNCTION(GetCameraNearClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "void set_nearClipPlane(float) property",
					asFUNCTION(SetCameraNearClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "float get_farClipPlane() const property",
					asFUNCTION(GetCameraFarClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "void set_farClipPlane(float) property",
					asFUNCTION(SetCameraFarClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "float get_aspect() const property",
					asFUNCTION(GetCameraAspect),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "void set_aspect(float) property",
					asFUNCTION(SetCameraAspect),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera", "bool get_isMain() const property",
					asFUNCTION(GetCameraIsMain),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera",
					"Vector3 WorldToViewportPoint("
						"const Vector3 &in worldPoint) const",
					asFUNCTION(CameraWorldToViewportPoint),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera",
					"Vector3 ViewportToWorldPoint("
						"const Vector3 &in viewportPoint) const",
					asFUNCTION(CameraViewportToWorldPoint),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera",
					"Vector3 WorldToScreenPoint("
						"const Vector3 &in worldPoint) const",
					asFUNCTION(CameraWorldToScreenPoint),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera",
					"Vector3 ScreenToWorldPoint("
						"const Vector3 &in screenPoint) const",
					asFUNCTION(CameraScreenToWorldPoint),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera",
					"Vector3 ScreenPointToDirection("
						"const Vector2 &in screenPoint) const",
					asFUNCTION(CameraScreenPointToDirection),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Camera",
					"void LookAt(const Vector3 &in worldPoint)",
					asFUNCTION(CameraLookAt),
					asCALL_CDECL_OBJLAST) >= 0;
			if (!registered)
			{
				error = "Could not register the Camera API.";
				return false;
			}

			engine.SetDefaultNamespace("Camera");
			const bool utilityRegistered =
				engine.RegisterGlobalFunction(
					"Camera@ get_main() property",
					asFUNCTION(GetMainCamera), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			if (utilityRegistered)
				return true;
			error = "Could not register the Camera utilities.";
			return false;
		}

		bool RegisterAudioSourceApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(
					engine, "AudioSource", error))
			{
				return false;
			}

			const bool registered =
				engine.RegisterObjectMethod(
					"AudioSource", "Component@+ opImplCast()",
					asFUNCTION(CastAudioSourceToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "AudioSource@+ opCast()",
					asFUNCTION(CastComponentToAudioSource),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource",
					"AudioClip@ get_clip() const property",
					asFUNCTION(GetAudioSourceClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource",
					"void set_clip(AudioClip@+) property",
					asFUNCTION(SetAudioSourceClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource",
					"bool get_is2D() const property",
					asFUNCTION(GetAudioSourceIs2D),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource",
					"void set_is2D(bool) property",
					asFUNCTION(SetAudioSourceIs2D),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource",
					"bool get_isPlaying() const property",
					asFUNCTION(GetAudioSourceIsPlaying),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource",
					"bool get_isPaused() const property",
					asFUNCTION(GetAudioSourceIsPaused),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource", "bool Play()",
					asFUNCTION(PlayAudioSource),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource", "bool Pause()",
					asFUNCTION(PauseAudioSource),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource", "bool Resume()",
					asFUNCTION(ResumeAudioSource),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"AudioSource", "void Stop()",
					asFUNCTION(StopAudioSource),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error = "Could not register the AudioSource API.";
			return false;
		}

		bool RegisterReziAudioApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(
					engine, "ReziAudioEmitter", error) ||
				!RegisterComponentHandle(
					engine, "ReziAudioListener", error))
			{
				return false;
			}

			const bool registered =
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"Component@+ opImplCast()",
					asFUNCTION(CastReziAudioEmitterToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component",
					"ReziAudioEmitter@+ opCast()",
					asFUNCTION(CastComponentToReziAudioEmitter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioListener",
					"Component@+ opImplCast()",
					asFUNCTION(CastReziAudioListenerToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component",
					"ReziAudioListener@+ opCast()",
					asFUNCTION(CastComponentToReziAudioListener),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"AudioClip@ get_clip() const property",
					asFUNCTION(GetReziAudioEmitterClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void set_clip(AudioClip@+) property",
					asFUNCTION(SetReziAudioEmitterClip),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"float get_volume() const property",
					asFUNCTION(GetReziAudioEmitterVolume),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void set_volume(float) property",
					asFUNCTION(SetReziAudioEmitterVolume),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"float get_pitch() const property",
					asFUNCTION(GetReziAudioEmitterPitch),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void set_pitch(float) property",
					asFUNCTION(SetReziAudioEmitterPitch),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool get_spatial() const property",
					asFUNCTION(GetReziAudioEmitterSpatial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void set_spatial(bool) property",
					asFUNCTION(SetReziAudioEmitterSpatial),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetFloatParameter(const string &in, float)",
					asFUNCTION(SetReziAudioFloatParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"float GetFloatParameter(const string &in, "
					"float fallback = 0.0f) const",
					asFUNCTION(GetReziAudioFloatParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetIntParameter(const string &in, int)",
					asFUNCTION(SetReziAudioIntParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"int GetIntParameter(const string &in, "
					"int fallback = 0) const",
					asFUNCTION(GetReziAudioIntParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetBoolParameter(const string &in, bool)",
					asFUNCTION(SetReziAudioBoolParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool GetBoolParameter(const string &in, "
					"bool fallback = false) const",
					asFUNCTION(GetReziAudioBoolParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetVector2Parameter(const string &in, "
					"const Vector2 &in)",
					asFUNCTION(SetReziAudioVector2Parameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"Vector2 GetVector2Parameter(const string &in, "
					"const Vector2 &in fallback) const",
					asFUNCTION(GetReziAudioVector2Parameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetVector3Parameter(const string &in, "
					"const Vector3 &in)",
					asFUNCTION(SetReziAudioVectorParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"Vector3 GetVector3Parameter(const string &in, "
					"const Vector3 &in fallback) const",
					asFUNCTION(GetReziAudioVectorParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetFloatArrayParameter(const string &in, "
					"const array<float>@+)",
					asFUNCTION(SetReziAudioFloatArrayParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"array<float>@ GetFloatArrayParameter("
					"const string &in) const",
					asFUNCTION(GetReziAudioFloatArrayParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetIntArrayParameter(const string &in, "
					"const array<int>@+)",
					asFUNCTION(SetReziAudioIntegerArrayParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"array<int>@ GetIntArrayParameter("
					"const string &in) const",
					asFUNCTION(GetReziAudioIntegerArrayParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetAudioClipParameter(const string &in, "
					"AudioClip@+)",
					asFUNCTION(SetReziAudioClipParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"AudioClip@ GetAudioClipParameter("
					"const string &in) const",
					asFUNCTION(GetReziAudioClipParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void SetAudioClipArrayParameter("
					"const string &in, "
					"const array<AudioClip@>@+)",
					asFUNCTION(SetReziAudioClipArrayParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"array<AudioClip@>@ GetAudioClipArrayParameter("
					"const string &in) const",
					asFUNCTION(GetReziAudioClipArrayParameter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"void ClearParameters()",
					asFUNCTION(ClearReziAudioParameters),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool get_isPlaying() const property",
					asFUNCTION(IsReziAudioEmitterPlaying),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool get_isPaused() const property",
					asFUNCTION(IsReziAudioEmitterPaused),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool get_isFinished() const property",
					asFUNCTION(IsReziAudioEmitterFinished),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter", "bool Play()",
					asFUNCTION(PlayReziAudioEmitter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool PlayWithFade(float durationSeconds)",
					asFUNCTION(PlayReziAudioEmitterWithFade),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter", "bool Pause()",
					asFUNCTION(PauseReziAudioEmitter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter", "bool Resume()",
					asFUNCTION(ResumeReziAudioEmitter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter", "void Stop()",
					asFUNCTION(StopReziAudioEmitter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool StopWithFade(float durationSeconds)",
					asFUNCTION(StopReziAudioEmitterWithFade),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool FadeTo(float targetVolume, "
					"float durationSeconds)",
					asFUNCTION(FadeReziAudioEmitterTo),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"bool Seek(float seconds)",
					asFUNCTION(SeekReziAudioEmitter),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"float get_playbackSeconds() const property",
					asFUNCTION(GetReziAudioPlaybackSeconds),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"float get_playbackLengthSeconds() "
					"const property",
					asFUNCTION(GetReziAudioPlaybackLengthSeconds),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"ReziAudioEmitter",
					"float get_playbackPercentage() "
					"const property",
					asFUNCTION(GetReziAudioPlaybackPercentage),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error = "Could not register the ReziAudio component API.";
			return false;
		}

		bool RegisterAnimationApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(
					engine, "Animation", error))
			{
				return false;
			}

			const bool registered =
				engine.RegisterObjectMethod(
					"Animation", "Component@+ opImplCast()",
					asFUNCTION(CastAnimationToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "Animation@+ opCast()",
					asFUNCTION(CastComponentToAnimation),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"AnimationStateMachine@ "
					"get_stateMachine() const property",
					asFUNCTION(GetAnimationStateMachine),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"void set_stateMachine("
					"AnimationStateMachine@+) property",
					asFUNCTION(SetAnimationStateMachine),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"string get_currentState() const property",
					asFUNCTION(GetAnimationCurrentState),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"bool get_isPlaying() const property",
					asFUNCTION(GetAnimationIsPlaying),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"float get_speed() const property",
					asFUNCTION(GetAnimationSpeed),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"void set_speed(float) property",
					asFUNCTION(SetAnimationSpeed),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"bool get_debugDraw() const property",
					asFUNCTION(GetAnimationDebugDraw),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"void set_debugDraw(bool) property",
					asFUNCTION(SetAnimationDebugDraw),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation", "bool Play()",
					asFUNCTION(PlayAnimation),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"bool PlayState("
					"const string &in state, "
					"uint blendMilliseconds = 0)",
					asFUNCTION(PlayAnimationState),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"bool SendTrigger("
					"const string &in trigger)",
					asFUNCTION(SendAnimationTrigger),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation", "bool Reset()",
					asFUNCTION(ResetAnimation),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation", "void Stop()",
					asFUNCTION(StopAnimation),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Animation",
					"bool IsInState("
					"const string &in state) const",
					asFUNCTION(IsAnimationInState),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error = "Could not register the Animation API.";
			return false;
		}

		bool RegisterAudioListenerApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(
					engine, "AudioListener", error))
			{
				return false;
			}

			const bool registered =
				engine.RegisterObjectMethod(
					"AudioListener", "Component@+ opImplCast()",
					asFUNCTION(CastAudioListenerToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "AudioListener@+ opCast()",
					asFUNCTION(CastComponentToAudioListener),
					asCALL_CDECL_OBJLAST) >= 0;
			if (registered)
				return true;
			error = "Could not register the AudioListener API.";
			return false;
		}

		bool RegisterRigidBodyApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(engine, "RigidBody", error))
				return false;

			const bool enums =
				engine.RegisterEnum("RigidBodyType") >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyType", "Sphere",
					ComponentRigidBody::body_sphere) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyType", "Box",
					ComponentRigidBody::body_box) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyType", "Capsule",
					ComponentRigidBody::body_capsule) >= 0 &&
				engine.RegisterEnum("RigidBodyBehaviour") >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyBehaviour", "Fixed",
					ComponentRigidBody::fixed) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyBehaviour", "Dynamic",
					ComponentRigidBody::dynamic) >= 0 &&
				engine.RegisterEnumValue(
					"RigidBodyBehaviour", "Kinematic",
					ComponentRigidBody::kinematic) >= 0;
			if (!enums)
			{
				error = "Could not register the RigidBody enums.";
				return false;
			}

			const bool baseApi =
				engine.RegisterObjectMethod(
					"RigidBody", "Component@+ opImplCast()",
					asFUNCTION(CastRigidBodyToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "RigidBody@+ opCast()",
					asFUNCTION(CastComponentToRigidBody),
					asCALL_CDECL_OBJLAST) >= 0;
			if (!baseApi)
			{
				error = "Could not register RigidBody component casts.";
				return false;
			}

#define EGE_REGISTER_RIGID_FLOAT(Property, Name) \
			engine.RegisterObjectMethod( \
				"RigidBody", "float get_" Property "() const property", \
				asFUNCTION(Get##Name), asCALL_CDECL_OBJLAST) >= 0 && \
			engine.RegisterObjectMethod( \
				"RigidBody", "void set_" Property "(float) property", \
				asFUNCTION(Set##Name), asCALL_CDECL_OBJLAST) >= 0

#define EGE_REGISTER_RIGID_VECTOR(Property, Name) \
			engine.RegisterObjectMethod( \
				"RigidBody", \
				"Vector3 get_" Property "() const property", \
				asFUNCTION(Get##Name), asCALL_CDECL_OBJLAST) >= 0 && \
			engine.RegisterObjectMethod( \
				"RigidBody", \
				"void set_" Property \
					"(const Vector3 &in) property", \
				asFUNCTION(Set##Name), asCALL_CDECL_OBJLAST) >= 0

#define EGE_REGISTER_RIGID_READONLY_VECTOR(Property, Name) \
			engine.RegisterObjectMethod( \
				"RigidBody", \
				"Vector3 get_" Property "() const property", \
				asFUNCTION(Get##Name), asCALL_CDECL_OBJLAST) >= 0

			const bool properties =
				engine.RegisterObjectMethod(
					"RigidBody",
					"RigidBodyType get_bodyType() const property",
					asFUNCTION(GetRigidBodyType),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_bodyType(RigidBodyType) property",
					asFUNCTION(SetRigidBodyType),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"RigidBodyBehaviour get_behaviour() const property",
					asFUNCTION(GetRigidBodyBehaviour),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_behaviour("
						"RigidBodyBehaviour) property",
					asFUNCTION(SetRigidBodyBehaviour),
					asCALL_CDECL_OBJLAST) >= 0 &&
				EGE_REGISTER_RIGID_FLOAT("mass", Mass) &&
				EGE_REGISTER_RIGID_FLOAT(
					"restitution", Restitution) &&
				EGE_REGISTER_RIGID_FLOAT("friction", Friction) &&
				EGE_REGISTER_RIGID_FLOAT(
					"rollingFriction", RollingFriction) &&
				EGE_REGISTER_RIGID_FLOAT(
					"linearDamping", LinearDamping) &&
				EGE_REGISTER_RIGID_FLOAT(
					"angularDamping", AngularDamping) &&
				EGE_REGISTER_RIGID_VECTOR(
					"linearFactor", LinearFactor) &&
				EGE_REGISTER_RIGID_VECTOR(
					"angularFactor", AngularFactor) &&
				EGE_REGISTER_RIGID_VECTOR(
					"sphereCenter", SphereCenter) &&
				EGE_REGISTER_RIGID_FLOAT(
					"sphereRadius", SphereRadius) &&
				EGE_REGISTER_RIGID_VECTOR(
					"boxCenter", BoxCenter) &&
				EGE_REGISTER_RIGID_VECTOR(
					"boxHalfExtents", BoxHalfExtents) &&
				EGE_REGISTER_RIGID_VECTOR(
					"boxRotation", BoxRotation) &&
				EGE_REGISTER_RIGID_VECTOR(
					"capsuleStart", CapsuleStart) &&
				EGE_REGISTER_RIGID_VECTOR(
					"capsuleEnd", CapsuleEnd) &&
				EGE_REGISTER_RIGID_FLOAT(
					"capsuleRadius", CapsuleRadius) &&
				EGE_REGISTER_RIGID_VECTOR(
					"linearVelocity", LinearVelocity) &&
				EGE_REGISTER_RIGID_VECTOR(
					"angularVelocity", AngularVelocity) &&
				EGE_REGISTER_RIGID_VECTOR("gravity", Gravity) &&
				EGE_REGISTER_RIGID_READONLY_VECTOR(
					"centerOfMass", CenterOfMass) &&
				EGE_REGISTER_RIGID_READONLY_VECTOR(
					"totalForce", TotalForce) &&
				EGE_REGISTER_RIGID_READONLY_VECTOR(
					"totalTorque", TotalTorque) &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"bool get_hasRuntimeBody() const property",
					asFUNCTION(GetHasRuntimeBody),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"bool get_isAwake() const property",
					asFUNCTION(GetIsAwake),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"bool get_useWorldGravity() const property",
					asFUNCTION(GetUseWorldGravity),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_useWorldGravity(bool) property",
					asFUNCTION(SetUseWorldGravity),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"bool get_isTrigger() const property",
					asFUNCTION(GetIsTrigger),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_isTrigger(bool) property",
					asFUNCTION(SetIsTrigger),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"uint get_collisionLayer() const property",
					asFUNCTION(GetCollisionLayer),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_collisionLayer(uint) property",
					asFUNCTION(SetCollisionLayer),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"uint get_collisionMask() const property",
					asFUNCTION(GetCollisionMask),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void set_collisionMask(uint) property",
					asFUNCTION(SetCollisionMask),
					asCALL_CDECL_OBJLAST) >= 0;

#undef EGE_REGISTER_RIGID_FLOAT
#undef EGE_REGISTER_RIGID_VECTOR
#undef EGE_REGISTER_RIGID_READONLY_VECTOR

			const bool methods =
				engine.RegisterObjectMethod(
					"RigidBody", "void WakeUp()",
					asFUNCTION(WakeUp),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody", "void Sleep()",
					asFUNCTION(Sleep),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody", "void ClearForces()",
					asFUNCTION(ClearForces),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyCentralForce("
						"const Vector3 &in force)",
					asFUNCTION(ApplyCentralForce),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyForce("
						"const Vector3 &in force, "
						"const Vector3 &in relativePosition)",
					asFUNCTION(ApplyForce),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyCentralImpulse("
						"const Vector3 &in impulse)",
					asFUNCTION(ApplyCentralImpulse),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyImpulse("
						"const Vector3 &in impulse, "
						"const Vector3 &in relativePosition)",
					asFUNCTION(ApplyImpulse),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyTorque("
						"const Vector3 &in torque)",
					asFUNCTION(ApplyTorque),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody",
					"void ApplyTorqueImpulse("
						"const Vector3 &in torque)",
					asFUNCTION(ApplyTorqueImpulse),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"RigidBody", "void RebuildBody()",
					asFUNCTION(RebuildBody),
					asCALL_CDECL_OBJLAST) >= 0;

			if (properties && methods)
				return true;
			error = "Could not register the complete RigidBody API.";
			return false;
		}

		bool RegisterColliderApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			if (!RegisterComponentHandle(engine, "Collider", error))
				return false;

			const bool registered =
				engine.RegisterEnum("ColliderShape") >= 0 &&
				engine.RegisterEnumValue(
					"ColliderShape", "Sphere",
					static_cast<int>(
						ComponentCollider::ShapeType::Sphere)) >= 0 &&
				engine.RegisterEnumValue(
					"ColliderShape", "Box",
					static_cast<int>(
						ComponentCollider::ShapeType::Box)) >= 0 &&
				engine.RegisterEnumValue(
					"ColliderShape", "Capsule",
					static_cast<int>(
						ComponentCollider::ShapeType::Capsule)) >= 0 &&
				engine.RegisterObjectMethod(
					"Collider", "Component@+ opImplCast()",
					asFUNCTION(CastColliderToComponent),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Component", "Collider@+ opCast()",
					asFUNCTION(CastComponentToCollider),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Collider",
					"ColliderShape get_shape() const property",
					asFUNCTION(GetColliderShape),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Collider",
					"void set_shape(ColliderShape) property",
					asFUNCTION(SetColliderShape),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Collider",
					"bool get_isTrigger() const property",
					asFUNCTION(GetColliderIsTrigger),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Collider",
					"void set_isTrigger(bool) property",
					asFUNCTION(SetColliderIsTrigger),
					asCALL_CDECL_OBJLAST) >= 0;
			if (!registered)
			{
				error = "Could not register Collider base properties.";
				return false;
			}

#define EGE_REGISTER_COLLIDER_VECTOR(Property, Name) \
			engine.RegisterObjectMethod( \
				"Collider", \
				"Vector3 get_" Property "() const property", \
				asFUNCTION(GetCollider##Name), \
				asCALL_CDECL_OBJLAST) >= 0 && \
			engine.RegisterObjectMethod( \
				"Collider", \
				"void set_" Property \
					"(const Vector3 &in) property", \
				asFUNCTION(SetCollider##Name), \
				asCALL_CDECL_OBJLAST) >= 0

#define EGE_REGISTER_COLLIDER_FLOAT(Property, Name) \
			engine.RegisterObjectMethod( \
				"Collider", \
				"float get_" Property "() const property", \
				asFUNCTION(GetCollider##Name), \
				asCALL_CDECL_OBJLAST) >= 0 && \
			engine.RegisterObjectMethod( \
				"Collider", \
				"void set_" Property "(float) property", \
				asFUNCTION(SetCollider##Name), \
				asCALL_CDECL_OBJLAST) >= 0

			const bool properties =
				EGE_REGISTER_COLLIDER_VECTOR(
					"sphereCenter", SphereCenter) &&
				EGE_REGISTER_COLLIDER_FLOAT(
					"sphereRadius", SphereRadius) &&
				EGE_REGISTER_COLLIDER_VECTOR(
					"boxCenter", BoxCenter) &&
				EGE_REGISTER_COLLIDER_VECTOR(
					"boxHalfExtents", BoxHalfExtents) &&
				EGE_REGISTER_COLLIDER_VECTOR(
					"boxRotation", BoxRotation) &&
				EGE_REGISTER_COLLIDER_VECTOR(
					"capsuleStart", CapsuleStart) &&
				EGE_REGISTER_COLLIDER_VECTOR(
					"capsuleEnd", CapsuleEnd) &&
				EGE_REGISTER_COLLIDER_FLOAT(
					"capsuleRadius", CapsuleRadius);

#undef EGE_REGISTER_COLLIDER_VECTOR
#undef EGE_REGISTER_COLLIDER_FLOAT

			if (properties)
				return true;
			error = "Could not register Collider shape properties.";
			return false;
		}

		bool ValidateTypedComponentApi(
			asIScriptEngine& engine,
			std::string& error)
		{
			constexpr const char* ModuleName =
				"__EGE_TypedComponentApiValidation";
			constexpr const char* Source = R"(
void ValidateTypedComponentApi(GameObject@ object)
{
    RigidBody@ first = object.GetComponent<RigidBody>();
    bool hasBody = object.HasComponent<RigidBody>();
    RigidBody@ found;
    bool foundBody = object.TryGetComponent<RigidBody>(found);
    array<RigidBody@>@ bodies =
        object.GetComponents<RigidBody@>();
    RigidBody@ added = object.AddComponent<RigidBody>();
    Collider@ collider = object.GetComponent<Collider>();
    array<Collider@>@ colliders =
        object.GetComponents<Collider@>();
    Collider@ addedCollider = object.AddComponent<Collider>();
    MeshRenderer@ meshRenderer =
        object.GetComponent<MeshRenderer>();
    MeshRenderer@ addedMeshRenderer =
        object.AddComponent<MeshRenderer>();
    Animation@ animator =
        object.GetComponent<Animation>();
    Animation@ addedAnimator =
        object.AddComponent<Animation>();
    AudioSource@ audioSource =
        object.GetComponent<AudioSource>();
    AudioSource@ addedAudioSource =
        object.AddComponent<AudioSource>();
    array<AudioSource@>@ audioSources =
        object.GetComponents<AudioSource@>();
    AudioListener@ audioListener =
        object.GetComponent<AudioListener>();
    AudioListener@ addedAudioListener =
        object.AddComponent<AudioListener>();
    ReziAudioEmitter@ reziEmitter =
        object.GetComponent<ReziAudioEmitter>();
    ReziAudioEmitter@ addedReziEmitter =
        object.AddComponent<ReziAudioEmitter>();
    array<ReziAudioEmitter@>@ reziEmitters =
        object.GetComponents<ReziAudioEmitter@>();
    reziEmitter.SetFloatParameter("Volume", 0.5f);
    float graphVolume =
        reziEmitter.GetFloatParameter("Volume", 1.0f);
    reziEmitter.SetIntParameter("Variation", 2);
    int graphVariation =
        reziEmitter.GetIntParameter("Variation", 0);
    reziEmitter.SetBoolParameter("Loop", true);
    bool graphLoop =
        reziEmitter.GetBoolParameter("Loop", false);
    reziEmitter.SetVector3Parameter(
        "Position", Vector3(1.0f, 2.0f, 3.0f));
    Vector3 fallbackPosition = Vector3(0.0f, 0.0f, 0.0f);
    Vector3 graphPosition =
        reziEmitter.GetVector3Parameter(
            "Position", fallbackPosition);
    reziEmitter.ClearParameters();
    ReziAudioListener@ reziListener =
        object.GetComponent<ReziAudioListener>();
    ReziAudioListener@ addedReziListener =
        object.AddComponent<ReziAudioListener>();
    Component@ component = added;
    Component@ colliderComponent = addedCollider;
    Component@ meshRendererComponent = addedMeshRenderer;
    Component@ animationComponent = addedAnimator;
    Component@ audioSourceComponent = addedAudioSource;
    Component@ audioListenerComponent = addedAudioListener;
    Component@ reziEmitterComponent = addedReziEmitter;
    Component@ reziListenerComponent = addedReziListener;
    RigidBody@ casted = cast<RigidBody>(component);
    Collider@ castedCollider = cast<Collider>(colliderComponent);
    MeshRenderer@ castedMeshRenderer =
        cast<MeshRenderer>(meshRendererComponent);
    Animation@ castedAnimation =
        cast<Animation>(animationComponent);
    AudioSource@ castedAudioSource =
        cast<AudioSource>(audioSourceComponent);
    AudioListener@ castedAudioListener =
        cast<AudioListener>(audioListenerComponent);
    ReziAudioEmitter@ castedReziEmitter =
        cast<ReziAudioEmitter>(reziEmitterComponent);
    ReziAudioListener@ castedReziListener =
        cast<ReziAudioListener>(reziListenerComponent);
    if (first !is null)
    {
        uint componentId = first.id;
        bool componentValid = first.valid;
        first.enabled = !first.enabled;
        string componentType = first.typeName;
        GameObject@ owner = first.gameObject;

        first.bodyType = RigidBodyType::Box;
        first.behaviour = RigidBodyBehaviour::Dynamic;
        first.mass = first.mass;
        first.restitution = first.restitution;
        first.friction = first.friction;
        first.rollingFriction = first.rollingFriction;
        first.linearDamping = first.linearDamping;
        first.angularDamping = first.angularDamping;
        first.linearFactor = first.linearFactor;
        first.angularFactor = first.angularFactor;
        first.sphereCenter = first.sphereCenter;
        first.sphereRadius = first.sphereRadius;
        first.boxCenter = first.boxCenter;
        first.boxHalfExtents = first.boxHalfExtents;
        first.boxRotation = first.boxRotation;
        first.capsuleStart = first.capsuleStart;
        first.capsuleEnd = first.capsuleEnd;
        first.capsuleRadius = first.capsuleRadius;
        first.linearVelocity = first.linearVelocity;
        first.angularVelocity = first.angularVelocity;
        first.gravity = first.gravity;
        Vector3 center = first.centerOfMass;
        Vector3 force = first.totalForce;
        Vector3 torque = first.totalTorque;
        bool hasRuntimeBody = first.hasRuntimeBody;
        bool awake = first.isAwake;
        first.useWorldGravity = first.useWorldGravity;
        first.isTrigger = first.isTrigger;
        first.collisionLayer = first.collisionLayer;
        first.collisionMask = first.collisionMask;

        first.WakeUp();
        first.Sleep();
        first.ClearForces();
        first.ApplyCentralForce(force);
        first.ApplyForce(force, center);
        first.ApplyCentralImpulse(force);
        first.ApplyImpulse(force, center);
        first.ApplyTorque(torque);
        first.ApplyTorqueImpulse(torque);
        first.RebuildBody();
    }
    if (addedCollider !is null)
    {
        addedCollider.shape = ColliderShape::Box;
        addedCollider.isTrigger = addedCollider.isTrigger;
        addedCollider.sphereCenter = addedCollider.sphereCenter;
        addedCollider.sphereRadius = addedCollider.sphereRadius;
        addedCollider.boxCenter = addedCollider.boxCenter;
        addedCollider.boxHalfExtents = addedCollider.boxHalfExtents;
        addedCollider.boxRotation = addedCollider.boxRotation;
        addedCollider.capsuleStart = addedCollider.capsuleStart;
        addedCollider.capsuleEnd = addedCollider.capsuleEnd;
        addedCollider.capsuleRadius = addedCollider.capsuleRadius;
    }
    if (addedMeshRenderer !is null)
    {
        Mesh@ mesh = addedMeshRenderer.mesh;
        Mesh@ explicitMesh = addedMeshRenderer.GetMesh();
        Material@ material = addedMeshRenderer.GetMaterial(0);
        array<Material@>@ materials =
            addedMeshRenderer.GetMaterials();
        uint materialCount = addedMeshRenderer.materialCount;
        @addedMeshRenderer.mesh = mesh;
        addedMeshRenderer.SetMesh(explicitMesh);
        @addedMeshRenderer.material = material;
        @addedMeshRenderer.materials = materials;
        addedMeshRenderer.SetMaterials(materials);
        addedMeshRenderer.SetMaterial(0, material);
        addedMeshRenderer.AddMaterial(material);
        addedMeshRenderer.RemoveMaterialAt(materialCount);
        addedMeshRenderer.ClearMaterials();
        addedMeshRenderer.visible = addedMeshRenderer.visible;
        addedMeshRenderer.castShadows =
            addedMeshRenderer.castShadows;

        Mesh@ foundMesh =
            Resources::FindMesh("Assets/Models/Test.mesh");
        Material@ foundMaterial =
            Resources::FindMaterial(
                "Assets/Materials/Test.material");
        Mesh@ meshById = Resources::GetMesh(
            foundMesh is null ? 0 : foundMesh.id);
        Material@ materialById = Resources::GetMaterial(
            foundMaterial is null ? 0 : foundMaterial.id);
        if (meshById !is null)
        {
            bool meshValid = meshById.valid;
            string meshName = meshById.name;
            string meshPath = meshById.path;
            uint vertices = meshById.vertexCount;
            uint indices = meshById.indexCount;
            Vector3 bounds = meshById.boundsSize;
        }
        if (materialById !is null)
        {
            bool materialValid = materialById.valid;
            string materialName = materialById.name;
            string materialPath = materialById.path;
            materialById.name = materialName;
            materialById.doubleSided = materialById.doubleSided;
            materialById.planarReflections =
                materialById.planarReflections;
            materialById.alphaCutoff = materialById.alphaCutoff;
            materialById.baseColor = materialById.baseColor;
            materialById.metallic = materialById.metallic;
            materialById.roughness = materialById.roughness;
            materialById.specularColor = materialById.specularColor;
            materialById.emissiveColor = materialById.emissiveColor;
            materialById.emissiveIntensity =
                materialById.emissiveIntensity;
            materialById.normalStrength = materialById.normalStrength;
            materialById.occlusionStrength =
                materialById.occlusionStrength;
            materialById.uvTilingX = materialById.uvTilingX;
            materialById.uvTilingY = materialById.uvTilingY;
            materialById.uvOffsetX = materialById.uvOffsetX;
            materialById.uvOffsetY = materialById.uvOffsetY;
            @materialById.baseColorTexture =
                materialById.baseColorTexture;
            @materialById.normalTexture =
                materialById.normalTexture;
        }

        Texture@ texture =
            Resources::FindTexture("Assets/Textures/Test.png");
        AudioClip@ audio =
            Resources::FindAudioClip("Assets/Audio/Test.wav");
        AnimationClip@ animation =
            Resources::FindAnimationClip(
                "Assets/Animation/Test.eduanim");
        Model@ model =
            Resources::FindModel("Assets/Models/Test.glb");
        AnimationStateMachine@ stateMachine =
            Resources::FindAnimationStateMachine(
                "Assets/Animation/Test.edustatemachine.json");
        if (texture !is null)
            texture.colorSpace = texture.colorSpace;
        if (audio !is null)
        {
            audio.loop = audio.loop;
            audio.volume = audio.volume;
            audio.pitch = audio.pitch;
            audio.spatial = audio.spatial;
            audio.minimumDistance = audio.minimumDistance;
            audio.maximumDistance = audio.maximumDistance;
        }
        if (animation !is null)
        {
            float duration = animation.duration;
            uint channelCount = animation.channelCount;
            uint morphChannelCount = animation.morphChannelCount;
            bool hasChannel = animation.HasChannel("Root");
            bool hasMorphChannel =
                animation.HasMorphChannel("Mesh");
        }
        if (model !is null)
        {
            uint nodes = model.nodeCount;
        }
        if (stateMachine !is null)
        {
            stateMachine.defaultNode = stateMachine.defaultNode;
            string defaultState = stateMachine.defaultState;
            uint clipCount = stateMachine.clipCount;
            uint stateCount = stateMachine.nodeCount;
            uint transitionCount =
                stateMachine.transitionCount;
            if (clipCount > 0)
            {
                string clipName = stateMachine.GetClipName(0);
                AnimationClip@ clip =
                    stateMachine.GetClip(0);
                bool loops = stateMachine.IsClipLooping(0);
                int clipIndex =
                    stateMachine.FindClip(clipName);
            }
            if (stateCount > 0)
            {
                string stateName =
                    stateMachine.GetStateName(0);
                string stateClip =
                    stateMachine.GetStateClipName(0);
                int stateIndex =
                    stateMachine.FindState(stateName);
                bool hasState =
                    stateMachine.HasState(stateName);
            }
            if (transitionCount > 0)
            {
                string source =
                    stateMachine.GetTransitionSource(0);
                string target =
                    stateMachine.GetTransitionTarget(0);
                string trigger =
                    stateMachine.GetTransitionTrigger(0);
                uint blend =
                    stateMachine.GetTransitionBlendMilliseconds(0);
                bool hasTrigger =
                    stateMachine.HasTrigger(trigger);
            }
        }
    }
    if (addedAnimator !is null)
    {
        AnimationStateMachine@ stateMachine =
            addedAnimator.stateMachine;
        @addedAnimator.stateMachine = stateMachine;
        string currentState = addedAnimator.currentState;
        bool isPlaying = addedAnimator.isPlaying;
        addedAnimator.speed = addedAnimator.speed;
        addedAnimator.debugDraw = addedAnimator.debugDraw;
        bool played = addedAnimator.Play();
        bool statePlayed =
            addedAnimator.PlayState("Idle", 100);
        bool triggered =
            addedAnimator.SendTrigger("Move");
        bool inState = addedAnimator.IsInState("Idle");
        bool reset = addedAnimator.Reset();
        addedAnimator.Stop();
    }
    if (addedAudioSource !is null)
    {
        AudioClip@ clip = addedAudioSource.clip;
        @addedAudioSource.clip = clip;
        addedAudioSource.is2D = addedAudioSource.is2D;
        bool playing = addedAudioSource.isPlaying;
        bool paused = addedAudioSource.isPaused;
        bool playAccepted = addedAudioSource.Play();
        bool pauseAccepted = addedAudioSource.Pause();
        bool resumeAccepted = addedAudioSource.Resume();
        addedAudioSource.Stop();
    }
    if (addedAudioListener !is null)
    {
        bool listenerValid = addedAudioListener.valid;
        addedAudioListener.enabled = addedAudioListener.enabled;
        GameObject@ listenerOwner = addedAudioListener.gameObject;
    }
    if (addedReziEmitter !is null)
    {
        AudioClip@ clip = addedReziEmitter.clip;
        @addedReziEmitter.clip = clip;
        addedReziEmitter.volume = addedReziEmitter.volume;
        addedReziEmitter.pitch = addedReziEmitter.pitch;
        addedReziEmitter.spatial = addedReziEmitter.spatial;
        addedReziEmitter.SetAudioClipParameter(
            "Impact", clip);
        AudioClip@ graphClip =
            addedReziEmitter.GetAudioClipParameter("Impact");
        Vector2 pan = Vector2(0.25f, 0.75f);
        addedReziEmitter.SetVector2Parameter("Pan2D", pan);
        pan = addedReziEmitter.GetVector2Parameter("Pan2D", pan);
        array<float> weights = { 1.0f, 2.0f, 3.0f };
        addedReziEmitter.SetFloatArrayParameter("Weights", weights);
        weights =
            addedReziEmitter.GetFloatArrayParameter("Weights");
        array<int> sequence = { 1, 3, 2 };
        addedReziEmitter.SetIntArrayParameter("Sequence", sequence);
        sequence =
            addedReziEmitter.GetIntArrayParameter("Sequence");
        array<AudioClip@> variations = { clip };
        addedReziEmitter.SetAudioClipArrayParameter(
            "Variations", variations);
        variations =
            addedReziEmitter.GetAudioClipArrayParameter("Variations");
        bool playing = addedReziEmitter.isPlaying;
        bool paused = addedReziEmitter.isPaused;
        bool finished = addedReziEmitter.isFinished;
        bool playAccepted = addedReziEmitter.Play();
        bool fadePlayAccepted =
            addedReziEmitter.PlayWithFade(0.1f);
        bool pauseAccepted = addedReziEmitter.Pause();
        bool resumeAccepted = addedReziEmitter.Resume();
        bool fadeAccepted =
            addedReziEmitter.FadeTo(0.5f, 0.1f);
        bool seekAccepted = addedReziEmitter.Seek(0.25f);
        float playbackSeconds =
            addedReziEmitter.playbackSeconds;
        float playbackLength =
            addedReziEmitter.playbackLengthSeconds;
        float playbackPercentage =
            addedReziEmitter.playbackPercentage;
        bool fadeStopAccepted =
            addedReziEmitter.StopWithFade(0.1f);
        addedReziEmitter.Stop();
    }
    if (addedReziListener !is null)
    {
        bool listenerValid = addedReziListener.valid;
        addedReziListener.enabled = addedReziListener.enabled;
        GameObject@ listenerOwner = addedReziListener.gameObject;
    }
    object.RemoveComponent(component);
    object.RemoveComponent(colliderComponent);
    object.RemoveComponent(meshRendererComponent);
    object.RemoveComponent(animationComponent);
    object.RemoveComponent(audioSourceComponent);
    object.RemoveComponent(audioListenerComponent);
    object.RemoveComponent(reziEmitterComponent);
    object.RemoveComponent(reziListenerComponent);
}
)";

			asIScriptModule* module =
				engine.GetModule(ModuleName, asGM_ALWAYS_CREATE);
			if (!module ||
				module->AddScriptSection(
					"TypedComponentApiValidation",
					Source) < 0 ||
				module->Build() < 0)
			{
				engine.DiscardModule(ModuleName);
				error =
					"The typed Component API did not pass its "
					"AngelScript compile validation.";
				return false;
			}

			engine.DiscardModule(ModuleName);
			return true;
		}
	}

	bool RegisterTypedComponentApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		const bool componentBaseRegistered =
			engine.RegisterObjectMethod(
				"Component", "bool get_enabled() const property",
				asFUNCTION(GetComponentEnabled),
				asCALL_CDECL_OBJLAST) >= 0 &&
			engine.RegisterObjectMethod(
				"Component", "void set_enabled(bool) property",
				asFUNCTION(SetComponentEnabled),
				asCALL_CDECL_OBJLAST) >= 0;
		if (!componentBaseRegistered)
		{
			error = "Could not register the Component base API.";
			return false;
		}

		if (!RegisterRigidBodyApi(engine, error))
			return false;
		if (!RegisterColliderApi(engine, error))
			return false;
		if (!RegisterScriptAssetApi(engine, error))
			return false;
		if (!RegisterMeshRendererApi(engine, error))
			return false;
		if (!RegisterCameraApi(engine, error))
			return false;
		if (!RegisterAnimationApi(engine, error))
			return false;
		if (!RegisterAudioSourceApi(engine, error))
			return false;
		if (!RegisterAudioListenerApi(engine, error))
			return false;
		if (!RegisterReziAudioApi(engine, error))
			return false;

		const bool registered =
			engine.RegisterObjectMethod(
				"GameObject",
				"T@ GetComponent<T>() const",
				asFUNCTION(GetComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"bool HasComponent<T>() const",
				asFUNCTION(HasComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"bool TryGetComponent<T>(T@&out component) const",
				asFUNCTION(TryGetComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"array<T>@ GetComponents<T>() const",
				asFUNCTION(GetComponentsTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"T@ AddComponent<T>()",
				asFUNCTION(AddComponentTemplate),
				asCALL_GENERIC) >= 0 &&
			engine.RegisterObjectMethod(
				"GameObject",
				"void RemoveComponent(Component@+ component)",
				asFUNCTION(RemoveComponent),
				asCALL_CDECL_OBJLAST) >= 0;
		if (registered)
			return ValidateTypedComponentApi(engine, error);

		error = "Could not register the typed GameObject component API.";
		return false;
	}
}
