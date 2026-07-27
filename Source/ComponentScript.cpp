#include "ComponentScript.h"

#include "Application.h"
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

	className_ = config->GetString("Class", "");
	storedState_ = EGE::LoadPropertyBag(*config, "Properties");
	EnsureInstance();
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->SetInstanceClass(instanceHandle_, className_, storedState_);
}

void ComponentScript::OnStart()
{
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
	EnsureInstance();
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->StartInstance(instanceHandle_);
}

void ComponentScript::OnUpdate(float deltaTime)
{
	if (EGE::ScriptRuntime* runtime = GetRuntime(); runtime && instanceHandle_)
		runtime->UpdateInstance(instanceHandle_, deltaTime);
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

void ComponentScript::SetScriptClass(const std::string& className)
{
	if (className_ == className)
		return;

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

void ComponentScript::EnsureInstance()
{
	if (instanceHandle_)
		return;
	if (EGE::ScriptRuntime* runtime = GetRuntime())
	{
		instanceHandle_ = runtime->CreateInstance(className_, storedState_);
		if (instanceHandle_ && App->IsPlay())
			runtime->StartInstance(instanceHandle_);
	}
}
