#include "ScriptObjectReference.h"

#include "../Application.h"
#include "../Component.h"
#include "../GameObject.h"
#include "../ModuleLevelManager.h"

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
			if (component && component->GetUID() == componentId_)
				return component;
		}
		return nullptr;
	}

	bool ScriptComponentReference::IsValid() const
	{
		return Resolve() != nullptr;
	}

}
