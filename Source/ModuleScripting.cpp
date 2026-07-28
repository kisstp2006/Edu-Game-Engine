#include "ModuleScripting.h"

#include "Application.h"
#include "Event.h"
#include "Globals.h"
#include "ModuleFileSystem.h"
#include "Scripting/ScriptBindings.h"

ModuleScripting::ModuleScripting(bool startEnabled)
	: Module("Scripting", startEnabled)
{
}

bool ModuleScripting::Init(Config*)
{
	runtime_.SetEditorBuild(App && App->IsEditor());
	std::string error;
	if (!runtime_.RegisterApi(
			"Engine.Input", EGE::RegisterEngineBindings, error))
	{
		LOG("Could not configure AngelScript engine bindings: %s", error.c_str());
		return false;
	}
	return runtime_.Initialize();
}

bool ModuleScripting::Start(Config*)
{
	return SynchronizeProject();
}

update_status ModuleScripting::PreUpdate(float)
{
	return SynchronizeProject() ? UPDATE_CONTINUE : UPDATE_ERROR;
}

update_status ModuleScripting::Update(float dt)
{
	runtime_.Tick(dt);
	return UPDATE_CONTINUE;
}

bool ModuleScripting::CleanUp()
{
	runtime_.Shutdown();
	LOG("AngelScript scripting runtime shut down");
	return true;
}

void ModuleScripting::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case Event::play:
			runtime_.EnterPlayMode();
			break;
		case Event::stop:
			runtime_.LeavePlayMode();
			break;
		case Event::pause:
			runtime_.PausePlayMode();
			break;
		case Event::unpause:
			runtime_.ResumePlayMode();
			break;
		default:
			break;
	}
}

void ModuleScripting::SetHotReloadEnabled(bool enabled)
{
	runtime_.SetHotReloadEnabled(enabled);
}

bool ModuleScripting::Reload()
{
	return runtime_.ForceReload();
}

EGE::ScriptRuntime& ModuleScripting::GetRuntime()
{
	return runtime_;
}

const EGE::ScriptRuntime& ModuleScripting::GetRuntime() const
{
	return runtime_;
}

bool ModuleScripting::SynchronizeProject()
{
	if (!App || !App->fs)
		return false;
	return runtime_.SetProjectRoot(App->fs->GetProjectRoot());
}
