#include "Globals.h"
#include "Application.h"
#include "ModuleEditor.h"
#include "ModuleWindow.h"
#include "ModuleFileSystem.h"
#include "ModuleLevelManager.h"
#include "ModuleEditorCamera.h"
#include "ModuleRenderer3D.h"
#include "ModuleInput.h"
#include "GameObject.h"
#include "DebugDraw.h"
#include "Config.h"
#include "OpenGL.h"
#include "Panel.h"
#include "PanelConsole.h"
#include "PanelGOTree.h"
#include "PanelProperties.h"
#include "PanelConfiguration.h"
#include "PanelAbout.h"
#include "PanelResources.h"
#include "Event.h"
#include "Project/Project.h"
#include "Settings/SettingsService.h"
#include "Settings/SettingsStore.h"
#include "EditorTheme.h"

#include "imgui_node_editor.h"

#include <filesystem>
#include <string.h>
#include <algorithm>

using namespace std;

#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"			      
#include "backends/imgui_impl_opengl3.h"

#include "Leaks.h"

namespace ed = ax::NodeEditor;

ModuleEditor::ModuleEditor(bool start_enabled) : Module("Editor", start_enabled)
{
	selected_file[0] = '\0';
	open_project_dialog.SetTitle("Open Edu Game Engine Project");
	open_project_dialog.SetTypeFilters({".egeproject"});
	project_location_dialog.SetTitle("Select Project Location");
}

// Destructor
ModuleEditor::~ModuleEditor()
{
}

// Called before render is available
bool ModuleEditor::Init(Config* config)
{
	LOG("Init editor gui with imgui lib version %s", ImGui::GetVersion());

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = "imgui.ini";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableSetMousePos | ImGuiConfigFlags_DockingEnable;  // Enable Keyboard Controls
	io.WantSetMousePos = true;
    ImGui_ImplSDL2_InitForOpenGL(App->window->GetWindow(), App->renderer3D->context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Apply the default immediately. Editor settings may override it later.
	EGE::EditorTheme::Apply("midnight", false);

	// create all panels
	
    tab_panels[TabPanelBottom].name = "Output";
    tab_panels[TabPanelLeft].name = "Hierarchy";
    tab_panels[TabPanelRight].name = "Inspector";

	tab_panels[TabPanelBottom].panels.push_back(console = new PanelConsole());
	tab_panels[TabPanelLeft].panels.push_back(tree = new PanelGOTree());
	tab_panels[TabPanelRight].panels.push_back(props = new PanelProperties());
	tab_panels[TabPanelRight].panels.push_back(conf = new PanelConfiguration());
	tab_panels[TabPanelRight].panels.push_back(about = new PanelAbout());
	tab_panels[TabPanelLeft].panels.push_back(res = new PanelResources());

	return true;
}

bool ModuleEditor::Start(Config * config)
{
    //conf->active = config->GetBool("ConfActive", true);
    //props->active = config->GetBool("PropsActive", true);

	OnResize(App->window->GetWidth(), App->window->GetHeight());

	return true;
}

void ModuleEditor::Save(Config* config) const 
{
}

update_status ModuleEditor::PreUpdate(float dt)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(App->window->GetWindow());
    ImGui::NewFrame();

    // \note: needed for guizmo (maybe ImGui_Impl has a bug)
    int mx, my;
	SDL_GetMouseState(&mx, &my);	 
    ImGui::GetIO().MousePos = ImVec2(float(mx), float(my));

    ImGuiIO& io = ImGui::GetIO();
	capture_keyboard = io.WantCaptureKeyboard;
	capture_mouse = io.WantCaptureMouse;

	return UPDATE_CONTINUE;
}

update_status ModuleEditor::Update(float dt)
{
	update_status ret = UPDATE_CONTINUE;

	static bool showcase = false;

    ImGuiWindowFlags window_flags =  ImGuiWindowFlags_NoDocking;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground;

	if (ImGui::Begin("DockSpace Demo", nullptr, window_flags))
	{
		// DockSpace
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("DockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), 0);
		}
	}
	ImGui::End();

	ImGui::PopStyleVar(2);
	
	// Main menu GUI
	if (draw_menu == true)
	{
		if (ImGui::BeginMainMenuBar())
		{
			bool selected = false;
			if (ImGui::BeginMenu("File"))
			{
				const bool can_change_project = App->IsStop();
				if (ImGui::MenuItem(
						"New Project...", nullptr, false,
						can_change_project))
				{
					open_new_project_popup = true;
				}
				if (ImGui::MenuItem(
						"Open Project...", nullptr, false,
						can_change_project))
				{
					const std::shared_ptr<const EGE::Project> project =
						App->GetActiveProject();
					if (project)
						open_project_dialog.SetPwd(
							project->GetProjectDirectory().parent_path());
					open_project_dialog.Open();
				}

				if (const std::shared_ptr<const EGE::Project> project =
						App->GetActiveProject())
				{
					ImGui::Separator();
					ImGui::TextDisabled(
						"Project: %s", project->GetName().c_str());
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Save Scene"))
					App->level->Save("level.json");

				if (ImGui::MenuItem("Quit", "ESC"))
					ret = UPDATE_STOP;

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Project Settings..."))
					show_project_settings = true;
				if (ImGui::MenuItem("Editor Settings..."))
					show_editor_settings = true;
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				if (ImGui::MenuItem("Gui Demo"))
					showcase = !showcase;

				if (ImGui::MenuItem("Documentation"))
					App->RequestBrowser("https://github.com/d0n3val/Edu-Game-Engine/wiki");

				if (ImGui::MenuItem("Download latest"))
					App->RequestBrowser("https://github.com/d0n3val/Edu-Game-Engine/releases");

				if (ImGui::MenuItem("Report a bug"))
					App->RequestBrowser("https://github.com/d0n3val/Edu-Game-Engine/issues");

				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}
	}

	DrawProjectDialogs();
	DrawSettingsWindow(show_project_settings, false);
	DrawSettingsWindow(show_editor_settings, true);

    for(uint i=0; i< TabPanelCount; ++i)
    {
        const TabPanel& tab = tab_panels[i];
        ImGui::SetNextWindowPos(ImVec2((float)tab.posx, (float)tab.posy), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2((float)tab.width, (float)tab.height), ImGuiCond_FirstUseEver);
        if(ImGui::Begin(tab.name, nullptr, ImGuiWindowFlags_NoFocusOnAppearing))
        {
            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
            {
                // Draw all active panels
                for (vector<Panel*>::const_iterator it = tab.panels.begin(); it != tab.panels.end(); ++it)
                {
                    Panel* panel = (*it);

                    if (ImGui::BeginTabItem(panel->GetName()))
                    {
                        if (panel->IsActive())
                        {
                            panel->Draw();
                        }

                        ImGui::EndTabItem();
                    }
                }

                ImGui::EndTabBar();
            }           
        }
		ImGui::End();
	}

    if (file_dialog == opened)
        LoadFile((file_dialog_filter.length() > 0) ? file_dialog_filter.c_str() : nullptr);
    else
        in_modal = false;

    // Show showcase ? 
    if (showcase)
    {
        ImGui::ShowDemoWindow();
        ImGui::ShowMetricsWindow();
    }

	return ret;
}

void ModuleEditor::DrawProjectDialogs()
{
	if (open_new_project_popup)
	{
		open_new_project_popup = false;
		if (new_project_location[0] == '\0')
		{
			std::filesystem::path location =
				std::filesystem::current_path().parent_path();
			bool hasConfiguredLocation = false;
			if (const EGE::SettingsService* settings = App->GetSettings();
				settings && settings->HasEditorSettings())
			{
				const std::string configuredLocation =
					settings->Editor().GetString(
						"projects.default_location", "");
				std::error_code locationError;
				if (!configuredLocation.empty() &&
					std::filesystem::is_directory(
						configuredLocation, locationError))
				{
					location = configuredLocation;
					hasConfiguredLocation = true;
				}
			}
			if (const std::shared_ptr<const EGE::Project> project =
					App->GetActiveProject())
			{
				if (!hasConfiguredLocation)
					location = project->GetProjectDirectory().parent_path();
			}

			const std::string location_text = location.string();
			strncpy_s(
				new_project_location, sizeof(new_project_location),
				location_text.c_str(), _TRUNCATE);
		}
		ImGui::OpenPopup("New Project");
	}

	if (ImGui::BeginPopupModal(
			"New Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputTextWithHint(
			"##ProjectName", "Project name",
			new_project_name, sizeof(new_project_name));

		ImGui::InputText(
			"##ProjectLocation", new_project_location,
			sizeof(new_project_location), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
		{
			project_location_dialog.SetPwd(new_project_location);
			project_location_dialog.Open();
		}

		const bool can_create =
			new_project_name[0] != '\0' &&
			new_project_location[0] != '\0';
		if (ImGui::Button("Create") && can_create)
		{
			const std::filesystem::path project_directory =
				std::filesystem::path(new_project_location) /
				new_project_name;
			if (!App->RequestCreateProject(
					project_directory, new_project_name))
			{
				SetProjectStatus(
					false, "Another project operation is already pending.");
			}
			new_project_name[0] = '\0';
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel"))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	project_location_dialog.Display();
	if (project_location_dialog.HasSelected())
	{
		const std::string selected_location =
			project_location_dialog.GetSelected().string();
		strncpy_s(
			new_project_location, sizeof(new_project_location),
			selected_location.c_str(), _TRUNCATE);
		project_location_dialog.ClearSelected();
	}

	open_project_dialog.Display();
	if (open_project_dialog.HasSelected())
	{
		if (!App->RequestOpenProject(
				open_project_dialog.GetSelected()))
		{
			SetProjectStatus(
				false, "Another project operation is already pending.");
		}
		open_project_dialog.ClearSelected();
	}

	if (open_project_status_popup)
	{
		open_project_status_popup = false;
		ImGui::OpenPopup("Project Status");
	}

	if (ImGui::BeginPopupModal(
			"Project Status", nullptr,
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		const ImVec4 color = project_status_success
			? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
			: ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
		ImGui::TextColored(
			color, "%s", project_status_success ? "Success" : "Error");
		ImGui::TextWrapped("%s", project_status_message.c_str());
		if (ImGui::Button("OK"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
}

void ModuleEditor::DrawSettingsWindow(
	bool& open, bool editorSettings)
{
	if (!open)
		return;

	EGE::SettingsService* service = App->GetSettings();
	if (!service || (editorSettings && !service->HasEditorSettings()))
	{
		open = false;
		return;
	}

	EGE::SettingsStore& store =
		editorSettings ? service->Editor() : service->Project();
	ImGui::SetNextWindowSize(ImVec2(620.0f, 620.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(store.GetTitle().c_str(), &open))
	{
		ImGui::End();
		return;
	}

	ImGui::TextDisabled(
		"%s", store.GetValuesPath().string().c_str());
	if (store.IsDirty())
	{
		ImGui::SameLine();
		ImGui::TextColored(
			ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "Modified");
	}
	ImGui::Separator();

	for (const EGE::SettingCategory& category : store.GetCategories())
	{
		if (!ImGui::CollapsingHeader(
				category.label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			continue;
		}

		ImGui::Indent();
		for (const EGE::SettingDefinition& definition :
			category.settings)
		{
			const EGE::SettingValue* storedValue =
				store.FindValue(definition.id);
			if (!storedValue)
				continue;

			ImGui::PushID(definition.id.c_str());
			bool changed = false;
			EGE::SettingValue value = *storedValue;

			switch (definition.type)
			{
			case EGE::SettingType::Boolean:
			{
				bool current = std::get<bool>(value);
				if (ImGui::Checkbox(
						definition.label.c_str(), &current))
				{
					value = current;
					changed = true;
				}
				break;
			}
			case EGE::SettingType::Integer:
			{
				int current = std::get<int>(value);
				if (definition.hasRange)
				{
					changed = ImGui::SliderInt(
						definition.label.c_str(), &current,
						static_cast<int>(definition.minimum),
						static_cast<int>(definition.maximum));
				}
				else
				{
					changed = ImGui::InputInt(
						definition.label.c_str(), &current,
						static_cast<int>(definition.step));
				}
				if (changed)
					value = current;
				break;
			}
			case EGE::SettingType::Number:
			{
				float current =
					static_cast<float>(std::get<double>(value));
				if (definition.hasRange)
				{
					changed = ImGui::SliderFloat(
						definition.label.c_str(), &current,
						static_cast<float>(definition.minimum),
						static_cast<float>(definition.maximum));
				}
				else
				{
					changed = ImGui::InputFloat(
						definition.label.c_str(), &current,
						static_cast<float>(definition.step));
				}
				if (changed)
					value = static_cast<double>(current);
				break;
			}
			case EGE::SettingType::String:
			{
				char text[512] = {};
				strncpy_s(
					text, sizeof(text),
					std::get<std::string>(value).c_str(), _TRUNCATE);
				if (ImGui::InputText(
						definition.label.c_str(), text, sizeof(text)))
				{
					value = std::string(text);
					changed = true;
				}
				break;
			}
			case EGE::SettingType::Enumeration:
			{
				const std::string currentValue =
					std::get<std::string>(value);
				const char* preview = currentValue.c_str();
				for (const EGE::SettingOption& option :
					definition.options)
				{
					if (option.value == currentValue)
					{
						preview = option.label.c_str();
						break;
					}
				}

				if (ImGui::BeginCombo(
						definition.label.c_str(), preview))
				{
					for (const EGE::SettingOption& option :
						definition.options)
					{
						const bool selected =
							option.value == currentValue;
						if (ImGui::Selectable(
								option.label.c_str(), selected))
						{
							value = option.value;
							changed = true;
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				break;
			}
			}

			if (!definition.description.empty())
			{
				ImGui::TextDisabled(
					"%s", definition.description.c_str());
			}
			if (definition.restartRequired)
			{
				ImGui::SameLine();
				ImGui::TextColored(
					ImVec4(0.95f, 0.75f, 0.25f, 1.0f),
					"(restart required)");
			}

			if (changed && store.SetValue(
					definition.id, std::move(value)))
			{
				App->ApplySettings();
			}
			ImGui::Spacing();
			ImGui::PopID();
		}
		ImGui::Unindent();
	}

	ImGui::Separator();
	if (ImGui::Button("Save"))
	{
		std::string error;
		if (!store.Save(error))
			SetProjectStatus(false, error);
		else
			SetProjectStatus(true, store.GetTitle() + " saved.");
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		std::string error;
		if (!store.ReloadValues(error))
			SetProjectStatus(false, error);
		else
			App->ApplySettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset to Defaults"))
	{
		store.ResetToDefaults();
		App->ApplySettings();
	}

	ImGui::End();
}

void ModuleEditor::ApplyAppearance(
	const std::string& theme, bool compact)
{
	EGE::EditorTheme::Apply(theme, compact);
}

// Called before quitting
bool ModuleEditor::CleanUp()
{
	LOG("Freeing editor gui");
					  
    for(uint i=0; i< TabPanelCount; ++i)
    {
        for (vector<Panel*>::iterator it = tab_panels[i].panels.begin(); it != tab_panels[i].panels.end(); ++it)
        {
            RELEASE(*it);
        }
        
        tab_panels[i].panels.clear();
    }


	console = nullptr; // fix a but of log comming when we already freed the panel

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

	return true;
}

void ModuleEditor::PrepareForProjectChange()
{
	ClearSelected();
	if (tree)
		tree->drag = nullptr;
	if (props)
		props->ResetProjectState();
	if (res)
		res->ResetProjectState();

	file_dialog = closed;
	selected_file[0] = '\0';
	open_project_dialog.Close();
	open_project_dialog.ClearSelected();
	project_location_dialog.Close();
	project_location_dialog.ClearSelected();
}

void ModuleEditor::SetProjectStatus(
	bool success, const std::string& message)
{
	project_status_success = success;
	project_status_message = message;
	open_project_status_popup = true;
}

void ModuleEditor::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
#ifndef _DEBUG
		case Event::play:
		case Event::unpause:
			draw_menu = false;
			console->active = false;
			tree->active = false;
            props->active = false;
            conf->active = false;
            res->active = false;
		break;
		case Event::stop:
		case Event::pause:
			draw_menu = true;
			console->active = true;
			tree->active = true;
            props->active = true;
            conf->active = true;
            res->active = true;
		break;
#endif
		case Event::gameobject_destroyed:
		{
			GameObject** go = std::get_if<GameObject*>(&selected);
			if (go)
			{
				selected = App->level->Validate(*go);
			}
			tree->drag = App->level->Validate(tree->drag);
		}
		break;
		case Event::window_resize:
			OnResize(event.point2d.x, event.point2d.y);
		break;
	}
}

void ModuleEditor::DrawDebug()
{
}

void ModuleEditor::OnResize(int width, int height)
{
    // \todo: Viewport
	tab_panels[TabPanelLeft].posx      = 2;
	tab_panels[TabPanelLeft].posy      = 21;
	tab_panels[TabPanelLeft].width     = 350;
	tab_panels[TabPanelLeft].height    = height- tab_panels[TabPanelLeft].posy;

	tab_panels[TabPanelBottom].posx     = tab_panels[TabPanelLeft].posx + tab_panels[TabPanelLeft].width;
	tab_panels[TabPanelBottom].height   = 225;
	tab_panels[TabPanelBottom].posy     = height - tab_panels[TabPanelBottom].height;
	tab_panels[TabPanelBottom].width    = width - tab_panels[TabPanelLeft].width - tab_panels[TabPanelRight].width;

    tab_panels[TabPanelRight].width  = 350;
    tab_panels[TabPanelRight].posy   = 21;
    tab_panels[TabPanelRight].posx   = width - tab_panels[TabPanelRight].width;
    tab_panels[TabPanelRight].height = height - tab_panels[TabPanelRight].posy;
}

void ModuleEditor::HandleInput(SDL_Event* event)
{
    //if(App->GetState() != Application::play)
    {
        ImGui_ImplSDL2_ProcessEvent(event);
    }
}

bool ModuleEditor::FileDialog(const char * extension, const char* from_folder)
{
	bool ret = true;

	switch (file_dialog)
	{
		case closed:
			selected_file[0] = '\0';
			file_dialog_filter = (extension) ? extension : "";
			file_dialog_origin = (from_folder) ? from_folder : "";
			file_dialog = opened;
		case opened:
			ret = false;
		break;
	}

	return ret;
}

const char * ModuleEditor::CloseFileDialog()
{
	if (file_dialog == ready_to_close)
	{
		file_dialog = closed;
		return selected_file[0] ? selected_file : nullptr;
	}
	return nullptr;
}

void ModuleEditor::Draw()
{
	// Debug Draw on selected GameObject

    ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_MakeCurrent(App->window->GetWindow(), App->renderer3D->context);
}

bool ModuleEditor::UsingMouse() const
{
	return capture_mouse;
	//return in_modal || ImGui::IsMouseHoveringAnyWindow() || (ImGui::IsAnyItemActive() && ImGui::IsMouseDragging());
}

bool ModuleEditor::UsingKeyboard() const
{
	return capture_keyboard;
}

void ModuleEditor::Log(const char * entry)
{
	if(console != nullptr)
		console->AddLog(entry);
}

void ModuleEditor::LogInputEvent(uint key, uint state)
{
	static char entry[512];
	static const char* states[] = { "IDLE", "DOWN", "REPEAT", "UP" };

	if (conf != nullptr)
	{
		if(key < 1000)
			sprintf_s(entry, 512, "Keybr: %02u - %s\n", key, states[state]);
		else
			sprintf_s(entry, 512, "Mouse: %02u - %s\n", key - 1000, states[state]);
		conf->AddInput(entry);
	}
}

void ModuleEditor::LogFPS(float fps, float ms)
{
	if (conf != nullptr)
		conf->AddFPS(fps, ms);
}

/*
void ModuleEditor::SetSelected(GameObject * selected, bool focus)
{
	
	selected.go = selected;
	if (selected != nullptr && focus == true)
	{
		float radius = selected->global_bbox.MinimalEnclosingSphere().r;
		App->camera->CenterOn(selected->GetGlobalPosition(), std::fmaxf(radius, 5.0f) * 3.0f);
		tree->open_selected = true;
	}
}
*/

void ModuleEditor::LoadFile(const char* filter_extension, const char* from_dir)
{
	ImGui::OpenPopup("Load File");
	if (ImGui::BeginPopupModal("Load File", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		in_modal = true;

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 5.0f);
        ImGui::BeginChild("File Browser", ImVec2(0,300), true);
		DrawDirectoryRecursive(from_dir, filter_extension);
        ImGui::EndChild();
        ImGui::PopStyleVar();

		ImGui::PushItemWidth(250.f);
		if (ImGui::InputText("##file_selector", selected_file, FILE_MAX, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
			file_dialog = ready_to_close;

		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Ok", ImVec2(50, 20)))
			file_dialog = ready_to_close;
		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(50, 20)))
		{
			file_dialog = ready_to_close;
			selected_file[0] = '\0';
		}

		ImGui::EndPopup();
	}
	else
    {
		in_modal = false;
    }
}

void ModuleEditor::DrawDirectoryRecursive(const char* directory, const char* filter_extension) 
{
	vector<string> files;
	vector<string> dirs;

	std::string dir((directory) ? directory : "");
	dir += "/";

	App->fs->DiscoverFiles(dir.c_str(), files, dirs);

	for (vector<string>::const_iterator it = dirs.begin(); it != dirs.end(); ++it)
	{
		if (ImGui::TreeNodeEx((dir + (*it)).c_str(), 0, "%s/", (*it).c_str()))
		{
			DrawDirectoryRecursive((dir + (*it)).c_str(), filter_extension);
			ImGui::TreePop();
		}
	}

	std::sort(files.begin(), files.end());

	for (vector<string>::const_iterator it = files.begin(); it != files.end(); ++it)
	{
		const string& str = *it;

		bool ok = true;

		if(filter_extension && str.substr(str.find_last_of(".") + 1) != filter_extension)
			ok = false;

		if (ok && ImGui::TreeNodeEx(str.c_str(), ImGuiTreeNodeFlags_Leaf))
		{
			if (ImGui::IsItemClicked()) {
				sprintf_s(selected_file, FILE_MAX, "%s%s", dir.c_str(), str.c_str());

				if (ImGui::IsMouseDoubleClicked(0))
					file_dialog = ready_to_close;
			}

			ImGui::TreePop();
		}
	}
}
