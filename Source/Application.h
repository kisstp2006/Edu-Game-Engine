#ifndef __APPLICATION_CPP__
#define __APPLICATION_CPP__

#include "Globals.h"
#include "EngineAPI.h"
#include <list>
#include <string>
#include <memory>
#include <filesystem>
#include "Timer.h"
#include "EngineTime.h"
#include "MathGeoLib/include/Algorithm/Random/LCG.h"

class Config;
class Module;
class ModuleHardware;
class ModuleFileSystem;
class ModuleWindow;
class ModuleInput;
class ModuleAudio;
class ModulePhysics3D;
class ModuleRenderer3D;
class ModuleEditorCamera;
class ModuleEditor;
class ModuleLevelManager;
class ModuleResources;
class ModuleAI;
class ModuleScripting;
class ModulePrograms;
class ModuleRenderer;
class ModuleHints;
class ModuleDebugDraw;
class ThreadPool;

struct Event;

namespace EGE
{
	class Project;
	class ProjectManager;
	class SettingsService;
}

class Application
{
public:
	enum State
	{
		play,
		stop,
		pause,
		waiting_play,
		waiting_stop,
		waiting_pause,
		waiting_unpause
	};

public:
	explicit Application(EngineMode mode);
	~Application();

	void ReadConfiguration(const Config& config);
	void SaveConfiguration(Config& config) const;

	bool Init();
	update_status Update();
	bool CleanUp();
	void DebugDraw();
	const char* GetAppName() const;
	void SetAppName(const char* name) ;
	const char* GetOrganizationName() const;
	void SetOrganizationName(const char* name) ;
	uint GetFramerateLimit() const;
	void SetFramerateLimit(uint max_framerate);
	void Log(const char* entry);
	void LoadPrefs();
	void SavePrefs() const;
	void RequestBrowser(const char* url) const;
	void BroadcastEvent(const Event& event);
	State GetState() const;
	LCG& Random();
	void Play();
	void Pause();
	void UnPause();
	void Stop();
	bool IsPlay() const;
	bool IsPause() const;
	bool IsStop() const;
	bool IsEditor() const { return mode == EngineMode::Editor; }
	std::shared_ptr<const EGE::Project> GetActiveProject() const;
	bool RequestCreateProject(
		const std::filesystem::path& project_directory,
		const std::string& project_name);
	bool RequestOpenProject(
		const std::filesystem::path& project_file);
	const std::filesystem::path& GetFallbackProjectFile() const;
	EGE::SettingsService* GetSettings();
	const EGE::SettingsService* GetSettings() const;
	void ApplySettings();
	EGE::TimeService& GetTime();
	const EGE::TimeService& GetTime() const;

	ThreadPool* getThreadPool() {return threadPool.get(); }
private:

	void PrepareUpdate();
	void FinishUpdate();
	bool InitializeProjectSystem();
	bool InitializeSettingsSystem();
	void ProcessPendingProjectChange();
	bool LoadProjectContent(const std::shared_ptr<EGE::Project>& project);
	void NotifyProjectChange(bool success, const std::string& message);

	enum class ProjectChangeAction
	{
		None,
		Create,
		Open
	};

	struct PendingProjectChange
	{
		ProjectChangeAction action = ProjectChangeAction::None;
		std::filesystem::path path;
		std::string name;
	};

public:

	LCG*	random = nullptr;

	ModuleHardware* hw = nullptr;
	ModuleFileSystem* fs = nullptr;
	ModuleWindow* window = nullptr;
	ModuleInput* input = nullptr;
	ModuleAudio* audio = nullptr;
	ModulePhysics3D* physics3D = nullptr;
	ModuleRenderer3D* renderer3D = nullptr;
	ModuleEditorCamera* camera = nullptr;
	ModuleEditor* editor = nullptr;
	ModuleLevelManager* level = nullptr;
	ModuleResources* resources = nullptr;
	ModuleAI* ai = nullptr;
	ModuleScripting* scripting = nullptr;
    ModulePrograms* programs = nullptr;
    ModuleRenderer* renderer = nullptr;
	ModuleHints* hints = nullptr;
    ModuleDebugDraw* debug_draw = nullptr;

private:

	Timer	ms_timer;
	Timer	fps_timer;
	Uint32	frames;
	float	dt;
	int		fps_counter;
	int		last_frame_ms;
	int		last_fps;
	int		capped_ms;

	std::list<Module*> modules;
	std::string log;
	std::string app_name;
	std::string organization_name;

	std::unique_ptr<ThreadPool> threadPool;
	std::unique_ptr<EGE::ProjectManager> project_manager;
	std::unique_ptr<EGE::SettingsService> settings_service;
	PendingProjectChange pending_project_change;
	std::filesystem::path fallback_project_file;
	EGE::TimeService time_service;

	State state = State::stop;
	EngineMode mode = EngineMode::Editor;
};

// Give App pointer access everywhere
extern Application* App;

#endif // __APPLICATION_CPP__
