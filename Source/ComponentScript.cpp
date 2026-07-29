#include "ComponentScript.h"

#include "Application.h"
#include "GameObject.h"
#include "ModuleScripting.h"
#include "Reflection/PropertySerializer.h"

ComponentScript::ComponentScript(GameObject* gameObject)
	: Component(gameObject, Types::Script)
{
	EnsureInstance();
}

ComponentScript::~ComponentScript()
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->DestroyInstance(instanceHandle_);
}

void ComponentScript::OnSave(Config& config) const
{
	config.AddString("ScriptAsset", assetId_.c_str());
	config.AddString("Class", className_.c_str());

	EGE::PropertyBag state = storedState_;
	if (const EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		state = runtime->CaptureInstanceState(instanceHandle_, true);
	EGE::SavePropertyBag(config, "Properties", state);
}

void ComponentScript::OnLoad(Config* config)
{
	if (!config)
		return;

	assetId_ = config->GetString("ScriptAsset", "");
	className_ = config->GetString("Class", "");
	storedState_ = EGE::LoadPropertyBag(*config, "Properties");
	ResolveScriptReference();
	EnsureInstance();
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->SetInstanceClass(instanceHandle_, className_, storedState_);
}

void ComponentScript::OnStart()
{
	ResolveScriptReference();
	EnsureInstance();
}

void ComponentScript::OnActivate()
{
	EnsureInstance();
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->EnableInstance(instanceHandle_);
}

void ComponentScript::OnDeActivate()
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->DisableInstance(instanceHandle_);
}

void ComponentScript::OnPlay()
{
	ResolveScriptReference();
	EnsureInstance();
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->StartInstance(instanceHandle_);
}

void ComponentScript::OnUpdate(float deltaTime)
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->UpdateInstance(instanceHandle_, deltaTime);
}

void ComponentScript::OnFixedUpdate(float deltaTime)
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->FixedUpdateInstance(instanceHandle_, deltaTime);
}

void ComponentScript::OnLateUpdate(float deltaTime)
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->LateUpdateInstance(instanceHandle_, deltaTime);
}

void ComponentScript::OnCollision(GameObject* other)
{
	if (!other)
		return;
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->CollisionInstance(instanceHandle_, other->GetUID());
}

void ComponentScript::OnPhysicsEvent(
	EGE::Physics::ContactPhase phase,
	const EGE::Physics::CollisionInfo& info)
{
	if (EGE::ScriptRuntime* runtime = GetRuntime();
		runtime && instanceHandle_)
	{
		runtime->PhysicsEventInstance(
			instanceHandle_,
			phase,
			info);
	}
}

void ComponentScript::OnStop()
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->StopInstance(instanceHandle_);
}

void ComponentScript::RemapSerializedReferences(
	const std::map<uint, uint>& gameObjectIds,
	const std::map<uint, uint>& componentIds)
{
	const auto remap = [](std::uint64_t id, const auto& ids)
	{
		if (id == 0)
			return std::uint64_t{0};
		const auto found = ids.find(static_cast<uint>(id));
		return found == ids.end()
			? std::uint64_t{0}
			: static_cast<std::uint64_t>(found->second);
	};

	for (EGE::PropertyState& property : storedState_)
	{
		if (auto* reference =
				std::get_if<EGE::GameObjectReferenceValue>(
					&property.value))
		{
			reference->objectId =
				remap(reference->objectId, gameObjectIds);
		}
		else if (auto* reference =
				 std::get_if<EGE::ComponentReferenceValue>(
					 &property.value))
		{
			reference->objectId =
				remap(reference->objectId, gameObjectIds);
			reference->componentId =
				remap(reference->componentId, componentIds);
			if (reference->objectId == 0 ||
				reference->componentId == 0)
			{
				reference->objectId = 0;
				reference->componentId = 0;
			}
		}
	}

	if (EGE::ScriptRuntime* runtime = GetRuntime();
		runtime && instanceHandle_)
	{
		runtime->SetInstanceOwnerId(
			instanceHandle_, game_object->GetUID());
		runtime->SetInstanceClass(
			instanceHandle_, className_, storedState_);
	}
}

const std::string& ComponentScript::GetScriptClass() const
{
	return className_;
}

const std::string& ComponentScript::GetScriptAssetId() const
{
	return assetId_;
}

void ComponentScript::SetScriptClass(const std::string& className)
{
	std::string assetId;
	if (EGE::ScriptRuntime* runtime = GetRuntime())
	{
		for (const EGE::ScriptClassInfo& script :
			runtime->GetAvailableClasses())
		{
			if (script.name == className)
			{
				assetId = script.assetId;
				break;
			}
		}
	}
	SetScriptReference(assetId, className);
}

void ComponentScript::SetScriptReference(
	const std::string& assetId,
	const std::string& className)
{
	if (assetId_ == assetId && className_ == className)
		return;

	assetId_ = assetId;
	className_ = className;
	storedState_.clear();
	EnsureInstance();
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->SetInstanceClass(instanceHandle_, className_);
}

void ComponentScript::RefreshScriptReference()
{
	ResolveScriptReference();
}

EGE::ScriptInstanceHandle ComponentScript::GetInstanceHandle() const
{
	return instanceHandle_;
}

bool ComponentScript::IsBound() const
{
	const EGE::ScriptRuntime* runtime = GetRuntime();
	return runtime && instanceHandle_ &&
		runtime->IsInstanceBound(instanceHandle_);
}

asIScriptObject* ComponentScript::AcquireScriptObject(
	const asITypeInfo& requestedType) const
{
	EGE::ScriptRuntime* runtime = GetRuntime();
	return runtime && instanceHandle_
		? runtime->AcquireInstanceObject(
			instanceHandle_, requestedType)
		: nullptr;
}

EGE::ScriptRuntime* ComponentScript::GetRuntime() const
{
	return App && App->scripting ? &App->scripting->GetRuntime() : nullptr;
}

void ComponentScript::ResolveScriptReference()
{
	EGE::ScriptRuntime* runtime = GetRuntime();
	if (!runtime)
		return;

	const std::string resolved = runtime->ResolveClass(assetId_, className_);
	if (resolved.empty() || resolved == className_)
		return;

	className_ = resolved;
	if (instanceHandle_ &&
		runtime->GetInstanceClassName(instanceHandle_) != className_)
	{
		runtime->SetInstanceClass(instanceHandle_, className_, storedState_);
	}
}

void ComponentScript::EnsureInstance()
{
	if (instanceHandle_)
	{
		if (EGE::ScriptRuntime* runtime = GetRuntime())
			runtime->SetInstanceOwnerId(
				instanceHandle_, game_object->GetUID());
		return;
	}
	if (EGE::ScriptRuntime* runtime = GetRuntime())
	{
		instanceHandle_ = runtime->CreateInstance(className_, storedState_);
		if (instanceHandle_)
			runtime->SetInstanceOwnerId(
				instanceHandle_, game_object->GetUID());
		if (instanceHandle_ && App->IsPlay())
			runtime->StartInstance(instanceHandle_);
	}
}
