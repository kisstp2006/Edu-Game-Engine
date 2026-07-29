#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../Component.h"
#include "../GameObject.h"
#include "../ModuleLevelManager.h"
#include "../ModuleResources.h"
#include "../Resource.h"

namespace EGE
{
	GameObject* ScriptGameObjectReference::Resolve() const
	{
		return App && App->level
			? App->level->Find(objectId_)
			: nullptr;
	}

	bool ScriptGameObjectReference::IsValid() const
	{
		return Resolve() != nullptr;
	}

	Component* ScriptComponentReference::Resolve() const
	{
		if (!App || !App->level)
			return nullptr;

		GameObject* owner = App->level->Find(objectId_);
		if (!owner)
			return nullptr;

		for (Component* component : owner->components)
		{
			if (component &&
				!component->flag_for_removal &&
				component->GetUID() == componentId_)
				return component;
		}
		return nullptr;
	}

	bool ScriptComponentReference::IsValid() const
	{
		return Resolve() != nullptr;
	}

	Resource* ScriptResourceReference::Resolve() const
	{
		if (!App || !App->resources)
			return nullptr;
		Resource* resource =
			App->resources->Get(resourceId_);
		return resource &&
			static_cast<int>(resource->GetType()) == resourceType_
				? resource
				: nullptr;
	}

	bool ScriptResourceReference::IsValid() const
	{
		return Resolve() != nullptr;
	}

	std::string ScriptResourceReference::GetName() const
	{
		const Resource* resource = Resolve();
		return resource && resource->GetUserResName()
			? resource->GetUserResName()
			: std::string();
	}

	std::string ScriptResourceReference::GetPath() const
	{
		const Resource* resource = Resolve();
		return resource && resource->GetFile()
			? resource->GetFile()
			: std::string();
	}
}
