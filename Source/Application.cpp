#include "Application.h"
#include "ModuleHardware.h"
#include "ModuleFileSystem.h"
#include "ModuleWindow.h"
#include "ModuleInput.h"
#include "ModuleAudio.h"
#include "ModulePhysics3D.h"
#include "ModuleRenderer3D.h"
#include "ModuleEditorCamera.h"
#include "ModuleEditor.h"
#include "ModuleLevelManager.h"
#include "ModuleResources.h"
#include "ModulePrograms.h"
#include "ModuleRenderer.h"
#include "ModuleHints.h"
#include "ModuleDebugDraw.h"
#include "ModuleAI.h"
#include "ModuleScripting.h"
#include "Event.h"
#include "Config.h"
#include "ThreadPool.h"
#include "GameObject.h"
#include "Project/ProjectManager.h"
#include "Project/ProjectSerializer.h"
#include "Settings/SettingsService.h"
#include "OpenGL.h"

#include <filesystem>

using namespace std;

// ---------------------------------------------
Application::Application(EngineMode mode) : mode(mode)
{
    threadPool = std::make_unique<ThreadPool>();
	project_manager = std::make_unique<EGE::ProjectManager>();

    frames = 0;
	last_frame_ms = -1;
	last_fps = -1;
	capped_ms = 1000 / 60;
	fps_counter = 0;
	random = new math::LCG();

	// The order of calls is very important!
	// Modules will Init() Start() and Update in this order
	// They will CleanUp() in reverse order

	modules.push_back(hints = new ModuleHints());
	modules.push_back(hw = new ModuleHardware(false));
	modules.push_back(fs = new ModuleFileSystem(ASSETS_FOLDER));
	modules.push_back(window = new ModuleWindow());
	modules.push_back(resources = new ModuleResources());
	modules.push_back(physics3D = new ModulePhysics3D());
	modules.push_back(camera = new ModuleEditorCamera());
	modules.push_back(renderer3D = new ModuleRenderer3D());
	modules.push_back(input = new ModuleInput());
	if (IsEditor())
		modules.push_back(editor = new ModuleEditor());
	modules.push_back(audio = new ModuleAudio(true));
	modules.push_back(ai = new ModuleAI());
	modules.push_back(scripting = new ModuleScripting());
	modules.push_back(level = new ModuleLevelManager());
    modules.push_back(programs = new ModulePrograms(true));
    modules.push_back(renderer = new ModuleRenderer());
	modules.push_back(debug_draw = new ModuleDebugDraw());
}

// ---------------------------------------------
Application::~Application()
{
	for(list<Module*>::reverse_iterator it = modules.rbegin(); it != modules.rend(); ++it)
		RELEASE(*it);

	RELEASE(random);
}

void Application::ReadConfiguration(const Config& config)
{
	app_name = config.GetString("Name", "Edu Engine");
	organization_name = config.GetString("Organization", "UPC CITM");
	SetFramerateLimit(config.GetInt("MaxFramerate", 0));
}

void Application::SaveConfiguration(Config& config) const
{
	config.AddString("Name", app_name.c_str());
	config.AddString("Organization", organization_name.c_str());
	config.AddInt("MaxFramerate", GetFramerateLimit());
}

// ---------------------------------------------
bool Application::Init()
{
	bool ret = true;

	if (!InitializeProjectSystem())
		return false;

	char* buffer = nullptr;
	fs->Load(SETTINGS_FOLDER "config.json", &buffer);

	Config config((const char*) buffer);

	ReadConfiguration(config.GetSection("App"));

	// We init everything, even if not anabled
	for (list<Module*>::iterator it = modules.begin(); it != modules.end() && ret; ++it)
	{
        Config section = config.GetSection((*it)->GetName());
		ret = (*it)->Init(&section); 
	}

	// Another round, just before starting the Updates. Only called for "active" modules
	// we send the configuration again in case a module needs it
	for(list<Module*>::iterator it = modules.begin(); it != modules.end() && ret; ++it)
	{
        if ((*it)->IsActive() == true)
        {
            Config section = config.GetSection((*it)->GetName());
            ret = (*it)->Start(&section);
		}
	}

	if (ret)
		ret = InitializeSettingsSystem();

	RELEASE_ARRAY(buffer);
	return ret;
}

bool Application::InitializeProjectSystem()
{
	std::error_code error;
	fallback_project_file = std::filesystem::absolute(
		"Fallback.egeproject", error).lexically_normal();
	if (error)
	{
		LOG("Could not resolve the fallback project path");
		return false;
	}

	const EGE::ProjectOpenResult opened =
		project_manager->OpenProject(fallback_project_file);
	if (!opened)
	{
		LOG("Could not open fallback project: %s",
			opened.status.message.c_str());
		return false;
	}

	if (!fs->SetProjectRoot(opened.project->GetProjectDirectory()))
	{
		LOG("Could not mount fallback project directory");
		return false;
	}

	LOG("Opened fallback project [%s]",
		opened.project->GetProjectFilePath().string().c_str());
	return true;
}

bool Application::InitializeSettingsSystem()
{
	settings_service = std::make_unique<EGE::SettingsService>();
	std::string error;
	if (!settings_service->Initialize(
			fs->GetFallbackRoot(), fs->GetProjectRoot(), IsEditor(), error))
	{
		LOG("Could not initialize settings: %s", error.c_str());
		settings_service.reset();
		return false;
	}

	ApplySettings();
	LOG("Loaded data-driven project%s settings",
		IsEditor() ? " and editor" : "");
	return true;
}

void Application::ProcessPendingProjectChange()
{
	if (pending_project_change.action == ProjectChangeAction::None)
		return;

	PendingProjectChange request = std::move(pending_project_change);
	pending_project_change = {};

	if (!IsStop())
	{
		NotifyProjectChange(
			false, "Stop Play Mode before changing projects.");
		return;
	}

	const std::shared_ptr<EGE::Project> previous_project =
		project_manager->GetActiveProject();
	if (!previous_project)
	{
		NotifyProjectChange(false, "There is no active project.");
		return;
	}

	std::string settings_error;
	if (settings_service && !settings_service->SaveAll(settings_error))
	{
		NotifyProjectChange(false, settings_error);
		return;
	}

	const bool save_resources =
		!settings_service || !settings_service->HasEditorSettings() ||
		settings_service->Editor().GetBool(
			"projects.save_before_switch", true);
	if (save_resources)
		resources->SaveResources();

	const EGE::ProjectStatus save_status =
		project_manager->SaveActiveProject();
	if (!save_status)
	{
		NotifyProjectChange(false, save_status.message);
		return;
	}

	EGE::ProjectOpenResult result;
	if (request.action == ProjectChangeAction::Create)
	{
		result = project_manager->CreateProject(
			request.path, request.name);
	}
	else
	{
		result = project_manager->OpenProject(request.path);
	}

	if (!result)
	{
		NotifyProjectChange(false, result.status.message);
		return;
	}

	if (result.project->GetProjectFilePath() ==
		previous_project->GetProjectFilePath())
	{
		NotifyProjectChange(
			true, "Project is already open: " + result.project->GetName());
		return;
	}

	// No old-frame GPU command may still reference scene-owned resources.
	glFinish();
	if (editor)
		editor->PrepareForProjectChange();

	level->UnloadCurrent();
	resources->UnloadProjectResources();

	if (!fs->SetProjectRoot(result.project->GetProjectDirectory()))
	{
		project_manager->ActivateProject(previous_project);
		resources->LoadProjectResources();
		LoadProjectContent(previous_project);
		NotifyProjectChange(
			false, "Could not mount the selected project. "
				"The previous project was restored.");
		return;
	}

	if (settings_service &&
		!settings_service->ChangeProject(
			result.project->GetProjectDirectory(), settings_error))
	{
		fs->SetProjectRoot(previous_project->GetProjectDirectory());
		project_manager->ActivateProject(previous_project);
		std::string restore_error;
		settings_service->ChangeProject(
			previous_project->GetProjectDirectory(), restore_error);
		resources->LoadProjectResources();
		LoadProjectContent(previous_project);
		ApplySettings();
		NotifyProjectChange(
			false, settings_error +
				" The previous project was restored.");
		return;
	}

	resources->LoadProjectResources();
	ApplySettings();
	const bool scene_loaded = LoadProjectContent(result.project);

	std::string title = "Edu Engine - " + result.project->GetName();
	window->SetTitle(title.c_str());

	if (scene_loaded)
	{
		NotifyProjectChange(
			true, "Project opened: " + result.project->GetName());
	}
	else
	{
		NotifyProjectChange(
			true, "Project opened with an empty scene because its "
				"start scene could not be loaded.");
	}
}

bool Application::LoadProjectContent(
	const std::shared_ptr<EGE::Project>& project)
{
	if (!project || project->GetConfig().startScene.empty())
	{
		level->CreateNewEmpty(
			project ? project->GetName().c_str() : "Untitled");
		return true;
	}

	std::error_code error;
	if (!std::filesystem::is_regular_file(
			project->GetStartScenePath(), error))
	{
		level->CreateNewEmpty(project->GetName().c_str());
		return false;
	}

	const std::string scene_path =
		project->GetConfig().startScene.generic_string();
	if (!level->Load(scene_path.c_str()))
	{
		level->CreateNewEmpty(project->GetName().c_str());
		return false;
	}

	return true;
}

void Application::NotifyProjectChange(
	bool success, const std::string& message)
{
	LOG("%s: %s", success ? "Project" : "Project error",
		message.c_str());
	if (editor)
		editor->SetProjectStatus(success, message);
}

// ---------------------------------------------
void Application::PrepareUpdate()
{
	ProcessPendingProjectChange();

	dt = (float)ms_timer.Read() / 1000.0f;
	ms_timer.Start();

	switch (state)
	{
		case waiting_play:
		{
			state = play;
			BroadcastEvent(Event(Event::EventType::play));
		} break;
		case waiting_stop:
		{
			state = stop;
			BroadcastEvent(Event(Event::EventType::stop));
		} break;
		case waiting_pause:
		{
			state = pause;
			BroadcastEvent(Event(Event::EventType::pause));
		} break;
		case waiting_unpause:
		{
			state = play;
			BroadcastEvent(Event(Event::EventType::unpause));
		} break;
	}
}

// ---------------------------------------------
update_status Application::Update()
{
	update_status ret = UPDATE_CONTINUE;
	PrepareUpdate();

	for(list<Module*>::iterator it = modules.begin(); it != modules.end() && ret == UPDATE_CONTINUE; ++it)
		if((*it)->IsActive() == true) 
			ret = (*it)->PreUpdate(dt);

	for(list<Module*>::iterator it = modules.begin(); it != modules.end() && ret == UPDATE_CONTINUE; ++it)
		if((*it)->IsActive() == true) 
			ret = (*it)->Update(dt);

	for(list<Module*>::iterator it = modules.begin(); it != modules.end() && ret == UPDATE_CONTINUE; ++it)
		if((*it)->IsActive() == true) 
			ret = (*it)->PostUpdate(dt);

	FinishUpdate();
	return ret;
}

// ---------------------------------------------
void Application::FinishUpdate()
{
	// Recap on framecount and fps
	++frames;
	++fps_counter;

	if(fps_timer.Read() >= 1000)
	{
		last_fps = fps_counter;
		fps_counter = 0;
		fps_timer.Start();
	}

	last_frame_ms = ms_timer.Read();

	// cap fps
	if(capped_ms > 0 && (last_frame_ms < capped_ms))
		SDL_Delay(capped_ms - last_frame_ms);

	// notify the editor
	if (editor)
		editor->LogFPS((float) last_fps, (float) last_frame_ms);
}

// ---------------------------------------------
bool Application::CleanUp()
{
	bool ret = true;

    fs->Save(SETTINGS_FOLDER "Engine.log", log.c_str(), uint(log.size()));
	SavePrefs();
	if (settings_service)
	{
		std::string settings_error;
		if (!settings_service->SaveAll(settings_error))
		{
			LOG("Could not save settings: %s", settings_error.c_str());
		}
	}

	for(list<Module*>::reverse_iterator it = modules.rbegin(); it != modules.rend() && ret; ++it)
		if((*it)->IsActive() == true) 
			ret = (*it)->CleanUp();

	return ret;
}

// ---------------------------------------------
void Application::DebugDraw()
{
	for (list<Module*>::iterator it = modules.begin(); it != modules.end(); ++it)
		if ((*it)->IsActive() == true)
			(*it)->DrawDebug();
}

// ---------------------------------------------
const char* Application::GetAppName() const
{
	return app_name.c_str();
}

// ---------------------------------------------
void Application::SetAppName(const char * name)
{
	if (name != nullptr && name != app_name)
	{
		app_name = name;
		window->SetTitle(name);
		// TODO: Filesystem should adjust its writing folder
	}
}

// ---------------------------------------------
const char* Application::GetOrganizationName() const
{
	return organization_name.c_str();
}

void Application::SetOrganizationName(const char * name)
{
	if (name != nullptr && name != organization_name)
	{
		organization_name = name;
		// TODO: Filesystem should adjust its writing folder
	}
}

// ---------------------------------------------
uint Application::GetFramerateLimit() const
{
	if(capped_ms > 0)
		return (uint) ((1.0f/(float)capped_ms) * 1000.0f);
	else
		return 0;
}

// ---------------------------------------------
void Application::SetFramerateLimit(uint max_framerate)
{				  
	if (max_framerate > 0)
		capped_ms = 1000 / max_framerate;
	else
		capped_ms = 0;
}

// ---------------------------------------------
void Application::Log(const char * entry)
{
	// save all logs, so we can dump all in a file upon close
	log.append(entry);

	// send to editor console
	if (editor)
		editor->Log(entry);
}

// ---------------------------------------------
void Application::LoadPrefs()
{
	char* buffer = nullptr;
	fs->Load(SETTINGS_FOLDER "config.json", &buffer);

	if (buffer != nullptr)
	{
		Config config((const char*)buffer);

		if (config.IsValid() == true)
		{
			LOG("Loading Engine Preferences");

			ReadConfiguration(config.GetSection("App"));

			Config section;
			for (list<Module*>::iterator it = modules.begin(); it != modules.end(); ++it)
			{
				section = config.GetSection((*it)->GetName());
				//if (section.IsValid())
					(*it)->Load(&section);
			}
		}
		else
			LOG("Cannot load Engine Preferences: Invalid format");

		RELEASE_ARRAY(buffer);
	}
}

// ---------------------------------------------
void Application::SavePrefs() const
{
	Config config;

    Config appCfg = config.AddSection("App");
	SaveConfiguration(appCfg);

    for (list<Module*>::const_iterator it = modules.begin(); it != modules.end(); ++it)
    {
        Config section = config.AddSection((*it)->GetName());
        (*it)->Save(&section);
    }

	char *buf;
	uint size = uint(config.Save(&buf, "Saved preferences for Edu Engine"));
	if(App->fs->Save(SETTINGS_FOLDER "config.json", buf, size) > 0)
		LOG("Saved Engine Preferences");
	RELEASE_ARRAY(buf);
}

// ---------------------------------------------
void Application::RequestBrowser(const char * url) const
{
   ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

// ---------------------------------------------
void Application::BroadcastEvent(const Event& event)
{
	for (list<Module*>::iterator it = modules.begin(); it != modules.end(); ++it)
		(*it)->ReceiveEvent(event);
}

// ---------------------------------------------
Application::State Application::GetState() const
{
	return state;
}

// ---------------------------------------------
LCG & Application::Random()
{
	return *random;
}

// ---------------------------------------------
void Application::Play()
{
	if (state == State::stop)
		state = State::waiting_play;
}

// ---------------------------------------------
void Application::Pause()
{
	if (state == State::play)
		state = State::waiting_pause;
}

// ---------------------------------------------
void Application::UnPause()
{
	if (state == State::pause)
		state = State::waiting_unpause;
}

// ---------------------------------------------
void Application::Stop()
{
	if (state == State::play || state == State::pause)
		state = State::waiting_stop;
}

bool Application::IsPlay() const
{
	return state == State::play;
}

bool Application::IsPause() const
{
	return state == State::pause;
}

bool Application::IsStop() const
{
	return state == State::stop;
}

std::shared_ptr<const EGE::Project> Application::GetActiveProject() const
{
	return project_manager ? project_manager->GetActiveProject() : nullptr;
}

bool Application::RequestCreateProject(
	const std::filesystem::path& project_directory,
	const std::string& project_name)
{
	if (!IsEditor() || pending_project_change.action !=
		ProjectChangeAction::None)
	{
		return false;
	}

	pending_project_change.action = ProjectChangeAction::Create;
	pending_project_change.path = project_directory;
	pending_project_change.name = project_name;
	return true;
}

bool Application::RequestOpenProject(
	const std::filesystem::path& project_file)
{
	if (!IsEditor() || pending_project_change.action !=
		ProjectChangeAction::None)
	{
		return false;
	}

	pending_project_change.action = ProjectChangeAction::Open;
	pending_project_change.path = project_file;
	pending_project_change.name.clear();
	return true;
}

EGE::SettingsService* Application::GetSettings()
{
	return settings_service.get();
}

const EGE::SettingsService* Application::GetSettings() const
{
	return settings_service.get();
}

void Application::ApplySettings()
{
	if (!settings_service)
		return;

	const EGE::SettingsStore& project = settings_service->Project();
	if (!IsEditor())
	{
		SetAppName(project.GetString(
			"application.display_name", GetAppName()).c_str());
		SetOrganizationName(project.GetString(
			"application.organization", GetOrganizationName()).c_str());
	}
	SetFramerateLimit(static_cast<uint>(
		project.GetInt("runtime.max_framerate", 0)));

	if (renderer3D)
		renderer3D->SetVSync(
			project.GetBool("rendering.vertical_sync", true));

	if (physics3D)
	{
		physics3D->SetGravity(float3(
			static_cast<float>(
				project.GetNumber("physics.gravity_x", 0.0)),
			static_cast<float>(
				project.GetNumber("physics.gravity_y", -10.0)),
			static_cast<float>(
				project.GetNumber("physics.gravity_z", 0.0))));
	}

	if (scripting)
	{
		scripting->SetHotReloadEnabled(project.GetBool(
			"scripting.hot_reload", IsEditor()));
	}

	if (IsEditor() && settings_service->HasEditorSettings())
	{
		const EGE::SettingsStore& editorSettings =
			settings_service->Editor();
		if (camera)
		{
			camera->mov_speed = static_cast<float>(
				editorSettings.GetNumber("camera.move_speed", 4.4));
			camera->rot_speed = static_cast<float>(
				editorSettings.GetNumber("camera.rotation_speed", 1.0));
			camera->zoom_speed = static_cast<float>(
				editorSettings.GetNumber("camera.zoom_speed", 1.5));
		}
		if (editor)
		{
			editor->ApplyAppearance(
				editorSettings.GetString("appearance.theme", "midnight"),
				editorSettings.GetBool(
					"appearance.compact_ui", false));
		}
	}

	if (window)
	{
		std::string title;
		if (IsEditor())
		{
			const std::shared_ptr<const EGE::Project> activeProject =
				GetActiveProject();
			title = "Edu Engine";
			if (activeProject)
				title += " - " + activeProject->GetName();
		}
		else
		{
			title = project.GetString(
				"application.display_name", "Edu Game");
		}
		window->SetTitle(title.c_str());
	}
}
