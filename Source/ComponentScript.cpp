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

void ComponentScript::OnStop()
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->StopInstance(instanceHandle_);
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
	if (instanceHandle_)
		runtime->SetInstanceClass(instanceHandle_, className_, storedState_);
}

void ComponentScript::EnsureInstance()
{
	if (instanceHandle_)
		return;
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
