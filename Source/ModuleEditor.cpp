#include "Globals.h"
#include "Application.h"
#include "EditorDialog.h"
#include "ModuleEditor.h"
#include "ModuleWindow.h"
#include "ModuleFileSystem.h"
#include "ModuleLevelManager.h"
#include "ModuleEditorCamera.h"
#include "ModuleRenderer3D.h"
#include "ModuleInput.h"
#include "ModuleResources.h"
#include "ModuleScripting.h"
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
#include "PanelAssets.h"
#include "Event.h"
#include "Project/Project.h"
#include "Project/RecentProjects.h"
#include "Project/VsCodeWorkspace.h"
#include "Settings/SettingsService.h"
#include "Settings/SettingsStore.h"
#include "EditorTheme.h"
#include "AssetEditorManager.h"

#include "imgui_node_editor.h"
#include "imgui_internal.h"

#include <filesystem>
#include <string.h>
#include <algorithm>
#include <cctype>

using namespace std;

#define IMGUI_IMPL_OPENGL_LOADER_GLEW
#include "imgui.h"
#include "backends/imgui_impl_sdl.h"			      
#include "backends/imgui_impl_opengl3.h"

#include "Leaks.h"

namespace ed = ax::NodeEditor;

namespace
{
	bool IsPathInside(
		const std::filesystem::path& child,
		const std::filesystem::path& parent)
	{
		const std::filesystem::path relative =
			child.lexically_relative(parent);
		if (relative.empty())
			return child == parent;
		const std::string text = relative.generic_string();
		return text != ".." && !text.starts_with("../");
	}

	std::string Lowercase(std::string value)
	{
		std::transform(
			value.begin(),
			value.end(),
			value.begin(),
			[](unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return value;
	}
}

ModuleEditor::ModuleEditor(bool start_enabled) : Module("Editor", start_enabled)
{
	selected_file[0] = '\0';
	open_project_dialog.SetTitle("Open Edu Game Engine Project");
	open_project_dialog.SetTypeFilters({".egeproject"});
	project_location_dialog.SetTitle("Select Project Location");
	open_scene_dialog.SetTitle("Open Scene");
	open_scene_dialog.SetTypeFilters({".eduscene", ".scene"});
	save_scene_dialog.SetTitle("Save Scene");
	save_scene_dialog.SetTypeFilters({".eduscene"});
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
	std::filesystem::path editorPreferenceDirectory;
	char* preferencePath =
		SDL_GetPrefPath("TiGames", "EduGameEngine");
	if (preferencePath)
	{
		editorPreferenceDirectory = preferencePath;
		SDL_free(preferencePath);
		imgui_ini_path =
			(editorPreferenceDirectory / "imgui.ini").string();
		io.IniFilename = imgui_ini_path.c_str();
	}
	else
	{
		io.IniFilename = nullptr;
		LOG("Could not resolve the editor preferences directory");
	}
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableSetMousePos | ImGuiConfigFlags_DockingEnable;  // Enable Keyboard Controls
	io.WantSetMousePos = true;
    ImGui_ImplSDL2_InitForOpenGL(App->window->GetWindow(), App->renderer3D->context);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // Apply the default immediately. Editor settings may override it later.
	EGE::EditorTheme::Apply("midnight", false);

	// create all panels
	
    tab_panels[TabPanelLeft].name = "Hierarchy";
    tab_panels[TabPanelRight].name = "Inspector";

	tab_panels[TabPanelBottom].panels.push_back(console = new PanelConsole());
	tab_panels[TabPanelLeft].panels.push_back(tree = new PanelGOTree());
	tab_panels[TabPanelRight].panels.push_back(props = new PanelProperties());
	tab_panels[TabPanelRight].panels.push_back(conf = new PanelConfiguration());
	tab_panels[TabPanelBottom].panels.push_back(assets = new PanelAssets());
	assetEditorManager = std::make_unique<EGE::AssetEditorManager>();
	recentProjects = std::make_unique<EGE::RecentProjects>();

	if (!editorPreferenceDirectory.empty())
	{
		std::string recentError;
		const std::filesystem::path recentFile =
			editorPreferenceDirectory /
			"RecentProjects.json";
		if (!recentProjects->Load(recentFile, recentError))
		{
			LOG("Could not load recent projects: %s",
				recentError.c_str());
		}
	}
	show_project_selector = App->GetActiveProject() == nullptr;

	return true;
}

bool ModuleEditor::Start(Config * config)
{
    //conf->active = config->GetBool("ConfActive", true);
    //props->active = config->GetBool("PropsActive", true);

	OnResize(App->window->GetWidth(), App->window->GetHeight());
	ResetSceneHistory();

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
			ImGuiID dockspace_id = ImGui::GetID("EditorDockSpaceV2");
			if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
			{
				BuildDefaultDockLayout(
					dockspace_id,
					ImGui::GetContentRegionAvail());
			}
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
						"Project Browser...", nullptr, false,
						can_change_project))
				{
					show_project_selector = true;
				}
				ImGui::Separator();
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
				if (ImGui::MenuItem(
						"Open in VS Code", nullptr, false,
						App->GetActiveProject() != nullptr))
				{
					OpenActiveProjectInVsCode();
				}

				if (const std::shared_ptr<const EGE::Project> project =
						App->GetActiveProject())
				{
					ImGui::Separator();
					ImGui::TextDisabled(
						"Project: %s", project->GetName().c_str());
				}

				ImGui::Separator();
				const bool can_edit_scene =
					App->GetActiveProject() != nullptr &&
					App->IsStop();
				if (ImGui::MenuItem(
						"New Scene", "Ctrl+N", false,
						can_edit_scene))
				{
					App->level->CreateNewEmpty("Untitled");
					ClearSelected();
					ResetSceneHistory();
				}
				if (ImGui::MenuItem(
						"Open Scene...", "Ctrl+O", false,
						can_edit_scene))
				{
					RequestOpenScene();
				}
				if (ImGui::MenuItem(
						"Save Scene", "Ctrl+S", false,
						can_edit_scene))
				{
					RequestSaveScene();
				}
				if (ImGui::MenuItem(
						"Save Scene As...", "Ctrl+Shift+S", false,
						can_edit_scene))
				{
					RequestSaveScene(true);
				}

				if (ImGui::MenuItem("Quit"))
					ret = UPDATE_STOP;

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				const char* undoLabel = GetUndoLabel();
				const std::string undoText = undoLabel
					? "Undo " + std::string(undoLabel)
					: "Undo";
				if (ImGui::MenuItem(
						undoText.c_str(), "Ctrl+Z", false, CanUndo()))
				{
					Undo();
				}
				const char* redoLabel = GetRedoLabel();
				const std::string redoText = redoLabel
					? "Redo " + std::string(redoLabel)
					: "Redo";
				if (ImGui::MenuItem(
						redoText.c_str(),
						"Ctrl+Y / Ctrl+Shift+Z",
						false,
						CanRedo()))
				{
					Redo();
				}
				ImGui::Separator();
				if (ImGui::MenuItem(
						"Project Settings...", nullptr, false,
						App->GetActiveProject() != nullptr))
					show_project_settings = true;
				if (ImGui::MenuItem("Editor Settings..."))
					show_editor_settings = true;
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Scripting"))
			{
				const bool hasProject =
					App->GetActiveProject() != nullptr &&
					App->scripting != nullptr;
				if (ImGui::MenuItem(
						"Reload Scripts", "Ctrl+R", false, hasProject))
				{
					App->scripting->Reload();
				}
				if (hasProject)
				{
					ImGui::Separator();
					ImGui::TextDisabled(
						"Hot reload: %s",
						App->scripting->GetRuntime().
							IsHotReloadEnabled()
							? "On"
							: "Off");
				}
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

	HandleEditorShortcuts();
	DrawProjectDialogs();
	DrawSceneDialogs();
	DrawSettingsWindow(show_project_settings, false);
	DrawSettingsWindow(show_editor_settings, true);
	if (DrawProjectSelector())
		ret = UPDATE_STOP;

	if (App->GetActiveProject())
	{
		DrawPanelGroup(TabPanelLeft);
		DrawPanelGroup(TabPanelRight);
		DrawStandalonePanels(TabPanelBottom);
		if (assetEditorManager)
			assetEditorManager->Draw();
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

void ModuleEditor::HandleEditorShortcuts()
{
	const ImGuiIO& io = ImGui::GetIO();
	if (!io.KeyCtrl ||
		io.WantTextInput ||
		ImGui::IsAnyItemActive() ||
		show_project_selector ||
		ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopup))
	{
		return;
	}

	const bool canEditScene =
		App->GetActiveProject() != nullptr && App->IsStop();
	if (canEditScene &&
		ImGui::IsKeyPressed(SDL_SCANCODE_Z, false))
	{
		if (io.KeyShift)
			Redo();
		else
			Undo();
		return;
	}
	if (canEditScene && !io.KeyShift &&
		ImGui::IsKeyPressed(SDL_SCANCODE_Y, false))
	{
		Redo();
		return;
	}
	if (canEditScene &&
		ImGui::IsKeyPressed(SDL_SCANCODE_S, false))
	{
		RequestSaveScene(io.KeyShift);
		return;
	}
	if (canEditScene && !io.KeyShift &&
		ImGui::IsKeyPressed(SDL_SCANCODE_O, false))
	{
		RequestOpenScene();
		return;
	}
	if (canEditScene && !io.KeyShift &&
		ImGui::IsKeyPressed(SDL_SCANCODE_N, false))
	{
		App->level->CreateNewEmpty("Untitled");
		ClearSelected();
		ResetSceneHistory();
		return;
	}
	if (App->GetActiveProject() &&
		App->scripting &&
		!io.KeyShift &&
		ImGui::IsKeyPressed(SDL_SCANCODE_R, false))
	{
		App->scripting->Reload();
	}
}

void ModuleEditor::DrawPanelGroup(TabPanelEnum group)
{
	const TabPanel& panelGroup = tab_panels[group];
	ImGui::SetNextWindowPos(
		ImVec2(
			static_cast<float>(panelGroup.posx),
			static_cast<float>(panelGroup.posy)),
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(
		ImVec2(
			static_cast<float>(panelGroup.width),
			static_cast<float>(panelGroup.height)),
		ImGuiCond_FirstUseEver);

	if (ImGui::Begin(
			panelGroup.name,
			nullptr,
			ImGuiWindowFlags_NoFocusOnAppearing))
	{
		if (ImGui::BeginTabBar("##PanelTabs"))
		{
			for (Panel* panel : panelGroup.panels)
			{
				if (ImGui::BeginTabItem(panel->GetName()))
				{
					if (panel->IsActive())
						panel->Draw();
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	ImGui::End();
}

void ModuleEditor::DrawStandalonePanels(TabPanelEnum group)
{
	const TabPanel& panelGroup = tab_panels[group];
	for (Panel* panel : panelGroup.panels)
	{
		ImGui::SetNextWindowPos(
			ImVec2(
				static_cast<float>(panelGroup.posx),
				static_cast<float>(panelGroup.posy)),
			ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(
			ImVec2(
				static_cast<float>(panelGroup.width),
				static_cast<float>(panelGroup.height)),
			ImGuiCond_FirstUseEver);

		if (ImGui::Begin(
				panel->GetName(),
				nullptr,
				ImGuiWindowFlags_NoFocusOnAppearing))
		{
			if (panel->IsActive())
				panel->Draw();
		}
		ImGui::End();
	}
}

void ModuleEditor::BuildDefaultDockLayout(
	ImGuiID dockspaceId,
	const ImVec2& dockspaceSize)
{
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(
		dockspaceId,
		ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodeSize(dockspaceId, dockspaceSize);

	ImGuiID centerDock = dockspaceId;
	ImGuiID hierarchyDock = 0;
	ImGuiID inspectorDock = 0;
	ImGuiID outputDock = 0;

	ImGui::DockBuilderSplitNode(
		centerDock,
		ImGuiDir_Left,
		0.18f,
		&hierarchyDock,
		&centerDock);
	ImGui::DockBuilderSplitNode(
		centerDock,
		ImGuiDir_Right,
		0.22f,
		&inspectorDock,
		&centerDock);
	ImGui::DockBuilderSplitNode(
		centerDock,
		ImGuiDir_Down,
		0.26f,
		&outputDock,
		&centerDock);

	ImGui::DockBuilderDockWindow("Hierarchy", hierarchyDock);
	ImGui::DockBuilderDockWindow("Inspector", inspectorDock);
	ImGui::DockBuilderDockWindow("Console", outputDock);
	ImGui::DockBuilderDockWindow("Assets", outputDock);
	ImGui::DockBuilderDockWindow("Viewport", centerDock);
	ImGui::DockBuilderFinish(dockspaceId);
}

bool ModuleEditor::DrawProjectSelector()
{
	if (!show_project_selector)
		return false;

	ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 workPosition = viewport->GetWorkPos();
	const ImVec2 workSize = viewport->GetWorkSize();
	const ImVec2 windowSize(
		std::min(720.0f, std::max(320.0f, workSize.x - 32.0f)),
		std::min(520.0f, std::max(320.0f, workSize.y - 32.0f)));
	ImGui::SetNextWindowPos(
		ImVec2(
			workPosition.x +
				(workSize.x - windowSize.x) * 0.5f,
			workPosition.y +
				(workSize.y - windowSize.y) * 0.5f),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings;
	if (!ImGui::Begin("Project Browser", nullptr, flags))
	{
		ImGui::End();
		return false;
	}

	ImGui::TextUnformatted("EDU GAME ENGINE");
	ImGui::TextDisabled(
		"Choose a project. Project assets are loaded only after selection.");
	ImGui::Separator();

	if (project_selection_pending)
	{
		ImGui::TextColored(
			ImVec4(0.35f, 0.72f, 1.0f, 1.0f),
			"Opening project...");
	}
	else
	{
		if (ImGui::Button("New Project", ImVec2(130.0f, 0.0f)))
			open_new_project_popup = true;
		ImGui::SameLine();
		if (ImGui::Button("Open Project", ImVec2(130.0f, 0.0f)))
		{
			if (!recentProjects ||
				recentProjects->GetEntries().empty())
			{
				open_project_dialog.SetPwd(
					App->GetFallbackProjectFile().parent_path());
			}
			else
			{
				open_project_dialog.SetPwd(
					recentProjects->GetEntries().front().
						projectFile.parent_path());
			}
			open_project_dialog.Open();
		}

		std::error_code fallbackError;
		const bool fallbackAvailable =
			std::filesystem::is_regular_file(
				App->GetFallbackProjectFile(), fallbackError);
		ImGui::SameLine();
		if (!fallbackAvailable)
		{
			ImGui::PushStyleVar(
				ImGuiStyleVar_Alpha,
				ImGui::GetStyle().Alpha * 0.45f);
		}
		const bool openFallback = ImGui::Button(
			"Open Fallback", ImVec2(130.0f, 0.0f));
		if (!fallbackAvailable)
			ImGui::PopStyleVar();
		if (openFallback)
		{
			if (fallbackAvailable)
			{
				RequestProjectFromSelector(
					App->GetFallbackProjectFile());
			}
			else
			{
				SetProjectStatus(
					false,
					"The fallback project file is unavailable.");
			}
		}
	}

	ImGui::Spacing();
	ImGui::TextDisabled("RECENT PROJECTS");
	ImGui::Separator();

	std::filesystem::path removeProject;
	const float recentProjectsHeight = std::max(
		150.0f,
		ImGui::GetContentRegionAvail().y -
			ImGui::GetFrameHeightWithSpacing() -
			ImGui::GetStyle().ItemSpacing.y);
	if (ImGui::BeginChild(
			"##RecentProjects",
			ImVec2(0.0f, recentProjectsHeight),
			true))
	{
		bool drewEntry = false;
		if (recentProjects)
		{
			const float recentProjectHeight =
				ImGui::GetFrameHeightWithSpacing() * 2.0f +
				ImGui::GetTextLineHeightWithSpacing() +
				ImGui::GetStyle().WindowPadding.y * 2.0f;
			for (const EGE::RecentProject& recent :
				recentProjects->GetEntries())
			{
				if (recent.projectFile ==
					App->GetFallbackProjectFile())
				{
					continue;
				}

				drewEntry = true;
				ImGui::PushID(
					recent.projectFile.string().c_str());
				if (ImGui::BeginChild(
						"##RecentProject",
						ImVec2(0.0f, recentProjectHeight),
						true,
						ImGuiWindowFlags_NoScrollbar |
							ImGuiWindowFlags_NoScrollWithMouse))
				{
					const bool activateRecent =
						ImGui::Selectable(
							recent.name.c_str(),
							false,
							ImGuiSelectableFlags_AllowDoubleClick) &&
						ImGui::IsMouseDoubleClicked(
							ImGuiMouseButton_Left);
					ImGui::TextDisabled(
						"%s", recent.projectFile.string().c_str());

					std::error_code projectError;
					const bool available =
						std::filesystem::is_regular_file(
							recent.projectFile, projectError);
					if (!available)
					{
						ImGui::TextColored(
							ImVec4(0.95f, 0.45f, 0.38f, 1.0f),
							"Project file is missing");
						ImGui::SameLine();
					}
					const bool canOpen =
						available && !project_selection_pending;
					if (!canOpen)
					{
						ImGui::PushStyleVar(
							ImGuiStyleVar_Alpha,
							ImGui::GetStyle().Alpha * 0.45f);
					}
					const bool openPressed = ImGui::Button("Open");
					if (!canOpen)
						ImGui::PopStyleVar();
					if ((openPressed || activateRecent) && canOpen)
					{
						RequestProjectFromSelector(
							recent.projectFile);
					}
					ImGui::SameLine();
					if (ImGui::Button("Remove"))
						removeProject = recent.projectFile;
				}
				ImGui::EndChild();
				ImGui::PopID();
			}
		}

		if (!drewEntry)
		{
			ImGui::TextDisabled(
				"No recent projects yet. Create one or open an "
				"existing .egeproject file.");
		}
	}
	ImGui::EndChild();

	if (!removeProject.empty() && recentProjects)
	{
		std::string removeError;
		if (!recentProjects->Remove(removeProject, removeError))
			SetProjectStatus(false, removeError);
	}

	bool quit = false;
	if (App->GetActiveProject())
	{
		if (ImGui::Button("Close", ImVec2(100.0f, 0.0f)))
			show_project_selector = false;
	}
	else if (ImGui::Button("Quit", ImVec2(100.0f, 0.0f)))
	{
		quit = true;
	}

	ImGui::End();
	return quit;
}

bool ModuleEditor::RequestProjectFromSelector(
	const std::filesystem::path& projectFile)
{
	if (project_selection_pending)
		return false;

	if (!App->RequestOpenProject(projectFile))
	{
		SetProjectStatus(
			false,
			"Another project operation is already pending.");
		return false;
	}

	project_selection_pending = true;
	return true;
}

void ModuleEditor::DrawProjectDialogs()
{
	bool focusProjectName = false;
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
		EGE::EditorDialog::Open("New Project");
		focusProjectName = true;
	}

	if (EGE::EditorDialog::Begin(
			"New Project",
			ImVec2(560.0f, 0.0f),
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextDisabled("Project name");
		if (focusProjectName)
			ImGui::SetKeyboardFocusHere();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint(
			"##ProjectName", "Project name",
			new_project_name, sizeof(new_project_name));

		ImGui::TextDisabled("Location");
		const float browseWidth =
			ImGui::CalcTextSize("Browse...").x +
			ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SetNextItemWidth(
			std::max(
				120.0f,
				ImGui::GetContentRegionAvail().x -
					browseWidth -
					ImGui::GetStyle().ItemSpacing.x));
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
		if (!can_create)
		{
			ImGui::PushStyleVar(
				ImGuiStyleVar_Alpha,
				ImGui::GetStyle().Alpha * 0.45f);
		}
		const bool createProject = ImGui::Button("Create");
		if (!can_create)
			ImGui::PopStyleVar();
		if (createProject && can_create)
		{
			const std::filesystem::path project_directory =
				std::filesystem::path(new_project_location) /
				new_project_name;
			if (App->RequestCreateProject(
					project_directory, new_project_name))
			{
				if (show_project_selector)
					project_selection_pending = true;
			}
			else
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
		if (ImGui::IsKeyPressed(
				ImGui::GetKeyIndex(ImGuiKey_Escape), false))
		{
			ImGui::CloseCurrentPopup();
		}

		project_location_dialog.Display();
		if (project_location_dialog.HasSelected())
		{
			const std::string selectedLocation =
				project_location_dialog.GetSelected().string();
			strncpy_s(
				new_project_location,
				sizeof(new_project_location),
				selectedLocation.c_str(),
				_TRUNCATE);
			project_location_dialog.ClearSelected();
		}

		EGE::EditorDialog::End();
	}

	open_project_dialog.Display();
	if (open_project_dialog.HasSelected())
	{
		const bool requested = show_project_selector
			? RequestProjectFromSelector(
				open_project_dialog.GetSelected())
			: App->RequestOpenProject(
				open_project_dialog.GetSelected());
		if (!requested && !show_project_selector)
		{
			SetProjectStatus(
				false, "Another project operation is already pending.");
		}
		open_project_dialog.ClearSelected();
	}

	if (open_project_status_popup)
	{
		open_project_status_popup = false;
		EGE::EditorDialog::Open("Project Status");
	}

	if (EGE::EditorDialog::Begin(
			"Project Status",
			ImVec2(420.0f, 0.0f),
			ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextColored(
			ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "Error");
		ImGui::TextWrapped("%s", project_status_message.c_str());
		if (ImGui::Button("OK") ||
			ImGui::IsKeyPressed(
				ImGui::GetKeyIndex(ImGuiKey_Enter), false) ||
			ImGui::IsKeyPressed(
				ImGui::GetKeyIndex(ImGuiKey_Escape), false))
		{
			ImGui::CloseCurrentPopup();
		}
		EGE::EditorDialog::End();
	}
}

void ModuleEditor::DrawSettingsWindow(
	bool& open, bool editorSettings)
{
	if (!open)
		return;

	EGE::SettingsService* service = App->GetSettings();
	if (!service ||
		(editorSettings && !service->HasEditorSettings()) ||
		(!editorSettings && !service->HasProjectSettings()))
	{
		open = false;
		return;
	}

	EGE::SettingsStore& store =
		editorSettings ? service->Editor() : service->Project();
	std::string& feedback =
		editorSettings
			? editor_settings_feedback
			: project_settings_feedback;
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
				feedback.clear();
				App->ApplySettings();
			}
			ImGui::Spacing();
			ImGui::PopID();
		}
		ImGui::Unindent();
	}

	if (editorSettings && ImGui::CollapsingHeader(
			"About", ImGuiTreeNodeFlags_DefaultOpen))
	{
		EGE::DrawAboutSection();
	}

	ImGui::Separator();
	if (ImGui::Button("Save"))
	{
		std::string error;
		if (!store.Save(error))
			SetProjectStatus(false, error);
		else
			feedback = "Saved.";
	}
	ImGui::SameLine();
	if (ImGui::Button("Reload"))
	{
		std::string error;
		if (!store.ReloadValues(error))
			SetProjectStatus(false, error);
		else
		{
			App->ApplySettings();
			feedback = "Reloaded from disk.";
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset to Defaults"))
	{
		store.ResetToDefaults();
		App->ApplySettings();
		feedback = "Defaults restored; save to keep these values.";
	}
	if (!feedback.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(
			ImVec4(0.40f, 0.78f, 0.62f, 1.0f),
			"%s",
			feedback.c_str());
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
	if (assetEditorManager)
	{
		assetEditorManager->CloseAll();
		assetEditorManager.reset();
	}
					  
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
	sceneHistory.Clear();
	historyBaseline = {};
	historyBaselineValid = false;
	historyTransactionEndRequested = false;
	historySuspended = false;
	if (assetEditorManager)
		assetEditorManager->CloseAll();
	ClearSelected();
	if (tree)
	{
		tree->drag = nullptr;
		tree->drag_candidate = nullptr;
	}
	if (props)
		props->ResetProjectState();
	if (tree)
		tree->ResetProjectState();
	if (assets)
		assets->ResetProjectState();

	file_dialog = closed;
	selected_file[0] = '\0';
	open_project_dialog.Close();
	open_project_dialog.ClearSelected();
	project_location_dialog.Close();
	project_location_dialog.ClearSelected();
	open_scene_dialog.Close();
	open_scene_dialog.ClearSelected();
	save_scene_dialog.Close();
	save_scene_dialog.ClearSelected();
}

bool ModuleEditor::OpenAssetEditor(
	const EGE::EditorAssetSelection& asset)
{
	if (!assetEditorManager)
		return false;

	std::string error;
	if (assetEditorManager->Open(asset, error))
		return true;

	SetProjectStatus(false, error);
	return false;
}

void ModuleEditor::CloseAssetEditors()
{
	if (assetEditorManager)
		assetEditorManager->CloseAll();
}

bool ModuleEditor::OpenSceneAsset(
	const std::filesystem::path& scenePath)
{
	if (!App->IsStop())
	{
		SetProjectStatus(
			false, "Stop the game before opening another scene.");
		return false;
	}

	const std::shared_ptr<const EGE::Project> project =
		App->GetActiveProject();
	if (!project)
	{
		SetProjectStatus(
			false, "There is no active project to open the scene from.");
		return false;
	}

	const std::filesystem::path projectDirectory =
		project->GetProjectDirectory().lexically_normal();
	const std::filesystem::path assetDirectory =
		project->GetAssetDirectory().lexically_normal();
	const std::filesystem::path absolutePath =
		(scenePath.is_absolute()
			? scenePath
			: projectDirectory / scenePath)
			.lexically_normal();
	const std::string extension =
		Lowercase(absolutePath.extension().string());

	std::error_code fileError;
	if (!IsPathInside(absolutePath, assetDirectory) ||
		(extension != ".eduscene" && extension != ".scene") ||
		!std::filesystem::is_regular_file(absolutePath, fileError))
	{
		SetProjectStatus(
			false,
			"Choose an existing .eduscene file inside the active "
			"project's asset directory.");
		return false;
	}

	const std::filesystem::path relativePath =
		absolutePath.lexically_relative(projectDirectory);
	if (!EGE::IsSafeProjectRelativePath(relativePath) ||
		!App->level->Load(relativePath.generic_string().c_str()))
	{
		SetProjectStatus(
			false, "The selected scene could not be loaded.");
		return false;
	}

	ClearSelected();
	ResetSceneHistory();
	return true;
}

void ModuleEditor::RequestOpenScene()
{
	const std::filesystem::path directory =
		GetSceneDialogDirectory();
	if (directory.empty())
	{
		SetProjectStatus(
			false, "There is no active project to open a scene from.");
		return;
	}

	open_scene_dialog.SetPwd(directory);
	open_scene_dialog.Open();
}

void ModuleEditor::RequestSaveScene(bool saveAs)
{
	if (!App->GetActiveProject())
	{
		SetProjectStatus(
			false, "There is no active project to save the scene into.");
		return;
	}

	if (!saveAs && App->level->HasScenePath())
	{
		if (!App->level->Save())
		{
			SetProjectStatus(false, "The scene could not be saved.");
			return;
		}

		App->resources->SaveResources();
		if (assets)
			assets->RefreshProjectAssets();
		AcceptCurrentSceneHistoryState();
		return;
	}

	const std::filesystem::path directory =
		GetSceneDialogDirectory();
	if (!directory.empty())
		save_scene_dialog.SetPwd(directory);
	save_scene_dialog.Open();
}

void ModuleEditor::DrawSceneDialogs()
{
	open_scene_dialog.Display();
	if (open_scene_dialog.HasSelected())
	{
		const std::filesystem::path selected =
			open_scene_dialog.GetSelected();
		open_scene_dialog.ClearSelected();

		const std::shared_ptr<const EGE::Project> project =
			App->GetActiveProject();
		if (!project)
		{
			SetProjectStatus(
				false, "The project was closed before the scene could open.");
		}
		else
		{
			OpenSceneAsset(selected);
		}
	}

	save_scene_dialog.Display();
	if (save_scene_dialog.HasSelected())
	{
		const std::filesystem::path selected =
			save_scene_dialog.GetSelected();
		save_scene_dialog.ClearSelected();
		SaveSceneTo(selected);
	}
}

bool ModuleEditor::SaveSceneTo(
	const std::filesystem::path& selectedPath)
{
	const std::shared_ptr<const EGE::Project> project =
		App->GetActiveProject();
	if (!project)
	{
		SetProjectStatus(
			false, "The project was closed before the scene could be saved.");
		return false;
	}

	std::filesystem::path absolutePath =
		std::filesystem::absolute(selectedPath).lexically_normal();
	if (Lowercase(absolutePath.extension().string()) != ".eduscene")
		absolutePath.replace_extension(".eduscene");

	const std::filesystem::path projectDirectory =
		project->GetProjectDirectory().lexically_normal();
	const std::filesystem::path assetDirectory =
		project->GetAssetDirectory().lexically_normal();
	if (!IsPathInside(absolutePath, assetDirectory))
	{
		SetProjectStatus(
			false,
			"Scenes must be saved inside the active project's "
			"asset directory.");
		return false;
	}

	const std::filesystem::path relativePath =
		absolutePath.lexically_relative(projectDirectory);
	if (!EGE::IsSafeProjectRelativePath(relativePath) ||
		!App->level->Save(relativePath.generic_string().c_str()))
	{
		SetProjectStatus(false, "The scene could not be saved.");
		return false;
	}

	App->resources->SaveResources();
	if (assets)
		assets->RefreshProjectAssets();
	AcceptCurrentSceneHistoryState();
	return true;
}

std::filesystem::path ModuleEditor::GetSceneDialogDirectory() const
{
	const std::shared_ptr<const EGE::Project> project =
		App->GetActiveProject();
	if (!project)
		return {};

	if (App->level->HasScenePath())
	{
		const std::filesystem::path currentDirectory =
			(project->GetProjectDirectory() /
			 App->level->GetScenePath()).parent_path();
		std::error_code error;
		if (std::filesystem::is_directory(currentDirectory, error))
			return currentDirectory;
	}

	return project->GetAssetDirectory();
}

void ModuleEditor::OpenActiveProjectInVsCode()
{
	const std::shared_ptr<const EGE::Project> project =
		App->GetActiveProject();
	if (!project)
	{
		SetProjectStatus(false, "There is no active project to open.");
		return;
	}

	std::string error;
	if (!EGE::OpenVsCode(project->GetProjectDirectory(), error))
		SetProjectStatus(false, error);
}

bool ModuleEditor::BeginSceneTransaction(
	const std::string& label)
{
	if (!App->GetActiveProject() ||
		!App->IsStop() ||
		sceneHistory.HasOpenTransaction())
	{
		return false;
	}

	EGE::EditorDocumentState current;
	if (!CaptureEditorDocumentState(current))
		return false;

	if (historyBaselineValid &&
		!historyBaseline.HasSameDocumentData(current))
	{
		sceneHistory.Push(
			"Edit Scene",
			historyBaseline,
			current);
	}
	historyBaseline = current;
	historyBaselineValid = true;
	sceneHistory.Begin(label, std::move(current));
	historyTransactionEndRequested = false;
	return true;
}

void ModuleEditor::EndSceneTransaction()
{
	if (sceneHistory.HasOpenTransaction())
		historyTransactionEndRequested = true;
}

void ModuleEditor::CancelSceneTransaction()
{
	sceneHistory.Cancel();
	historyTransactionEndRequested = false;
}

void ModuleEditor::SynchronizeSceneHistory()
{
	if (!App->GetActiveProject() || !App->IsStop())
	{
		if (sceneHistory.HasOpenTransaction())
			CancelSceneTransaction();
		historySuspended = true;
		return;
	}

	EGE::EditorDocumentState current;
	if (!CaptureEditorDocumentState(current))
		return;

	if (historySuspended || !historyBaselineValid)
	{
		historySuspended = false;
		historyBaseline = std::move(current);
		historyBaselineValid = true;
		return;
	}

	if (!sceneHistory.HasOpenTransaction() &&
		historyBaseline.documentType == current.documentType &&
		historyBaseline.payload == current.payload &&
		historyBaseline.documentId != current.documentId)
	{
		sceneHistory.RebaseDocument(
			current.documentType,
			current.documentId);
		historyBaseline = std::move(current);
		return;
	}

	if (sceneHistory.HasOpenTransaction())
	{
		if (!historyTransactionEndRequested)
			return;
		sceneHistory.Commit(current);
		historyTransactionEndRequested = false;
		historyBaseline = std::move(current);
		return;
	}

	if (!historyBaseline.HasSameDocumentData(current))
	{
		sceneHistory.Push(
			"Edit Scene",
			historyBaseline,
			current);
	}
	historyBaseline = std::move(current);
}

void ModuleEditor::ResetSceneHistory()
{
	sceneHistory.Clear();
	historyTransactionEndRequested = false;
	historySuspended = false;
	historyBaseline = {};
	historyBaselineValid =
		CaptureEditorDocumentState(historyBaseline);
}

bool ModuleEditor::Undo()
{
	const EGE::EditorDocumentState* state = sceneHistory.Undo();
	if (!state)
		return false;

	const EGE::EditorDocumentState restored = *state;
	if (!ApplyEditorDocumentState(restored))
	{
		sceneHistory.Redo();
		return false;
	}
	historyBaseline = restored;
	historyBaselineValid = true;
	return true;
}

bool ModuleEditor::Redo()
{
	const EGE::EditorDocumentState* state = sceneHistory.Redo();
	if (!state)
		return false;

	const EGE::EditorDocumentState restored = *state;
	if (!ApplyEditorDocumentState(restored))
	{
		sceneHistory.Undo();
		return false;
	}
	historyBaseline = restored;
	historyBaselineValid = true;
	return true;
}

bool ModuleEditor::CanUndo() const
{
	return App->GetActiveProject() &&
		App->IsStop() &&
		sceneHistory.CanUndo();
}

bool ModuleEditor::CanRedo() const
{
	return App->GetActiveProject() &&
		App->IsStop() &&
		sceneHistory.CanRedo();
}

const char* ModuleEditor::GetUndoLabel() const
{
	return CanUndo() ? sceneHistory.GetUndoLabel() : nullptr;
}

const char* ModuleEditor::GetRedoLabel() const
{
	return CanRedo() ? sceneHistory.GetRedoLabel() : nullptr;
}

bool ModuleEditor::CaptureEditorDocumentState(
	EGE::EditorDocumentState& state) const
{
	if (!App || !App->level || !App->GetActiveProject())
		return false;

	state = {};
	state.documentType = "Scene";
	state.documentId =
		App->level->GetScenePath().generic_string();
	if (!App->level->CaptureSceneSnapshot(state.payload))
		return false;

	if (const EGE::GameObjectSelection* selection =
			GetGameObjectSelection())
	{
		state.selectedObjects.reserve(selection->objects.size());
		for (GameObject* gameObject : selection->objects)
		{
			if (GameObject* valid =
					App->level->Validate(gameObject))
			{
				state.selectedObjects.push_back(valid->GetUID());
			}
		}
		if (GameObject* primary =
				App->level->Validate(selection->primary))
		{
			state.primaryObject = primary->GetUID();
		}
	}
	return true;
}

bool ModuleEditor::ApplyEditorDocumentState(
	const EGE::EditorDocumentState& state)
{
	if (state.documentType != "Scene" ||
		!App->IsStop())
	{
		return false;
	}

	ClearSelected();
	if (tree)
	{
		tree->drag = nullptr;
		tree->drag_candidate = nullptr;
	}
	if (!App->level->RestoreSceneSnapshot(
			state.payload,
			std::filesystem::path(state.documentId)))
	{
		return false;
	}

	std::vector<GameObject*> selection;
	selection.reserve(state.selectedObjects.size());
	for (uint uid : state.selectedObjects)
	{
		if (GameObject* gameObject = App->level->Find(uid))
			selection.push_back(gameObject);
	}
	GameObject* primary = state.primaryObject != 0
		? App->level->Find(state.primaryObject)
		: nullptr;
	SetGameObjectSelection(std::move(selection), primary);
	return true;
}

void ModuleEditor::AcceptCurrentSceneHistoryState()
{
	EGE::EditorDocumentState current;
	if (!CaptureEditorDocumentState(current))
		return;

	sceneHistory.RebaseDocument(
		current.documentType,
		current.documentId);
	historyBaseline = std::move(current);
	historyBaselineValid = true;
}

void ModuleEditor::NotifySelectionChanged()
{
	if (props)
		props->OnEditorSelectionChanged();
}

void ModuleEditor::SetProjectStatus(
	bool success, const std::string& message)
{
	project_selection_pending = false;
	if (success)
		return;

	project_status_message = message;
	open_project_status_popup = true;
}

void ModuleEditor::RecordRecentProject(
	const EGE::Project& project)
{
	project_selection_pending = false;
	show_project_selector = false;
	if (!recentProjects)
		return;

	std::string error;
	if (!recentProjects->Add(project, error))
		LOG("Could not save recent project: %s", error.c_str());
}

void ModuleEditor::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case Event::gameobject_destroyed:
		{
			if (const EGE::GameObjectSelection* selection =
					std::get_if<EGE::GameObjectSelection>(&selected))
			{
				SetGameObjectSelection(
					selection->objects,
					selection->primary);
			}
			tree->drag = App->level->Validate(tree->drag);
			tree->drag_candidate =
				App->level->Validate(tree->drag_candidate);
		}
		break;
		case Event::window_resize:
			OnResize(event.point2d.x, event.point2d.y);
		break;
	}
}

const EGE::GameObjectSelection*
ModuleEditor::GetGameObjectSelection() const
{
	return std::get_if<EGE::GameObjectSelection>(&selected);
}

GameObject* ModuleEditor::GetPrimaryGameObject() const
{
	const EGE::GameObjectSelection* selection =
		GetGameObjectSelection();
	return selection ? selection->primary : nullptr;
}

bool ModuleEditor::IsGameObjectSelected(
	const GameObject* gameObject) const
{
	const EGE::GameObjectSelection* selection =
		GetGameObjectSelection();
	return selection && std::find(
		selection->objects.begin(),
		selection->objects.end(),
		gameObject) != selection->objects.end();
}

void ModuleEditor::SetSelected(GameObject* gameObject)
{
	SetGameObjectSelection(
		gameObject ? std::vector<GameObject*>{gameObject}
				   : std::vector<GameObject*>{},
		gameObject);
}

void ModuleEditor::SetGameObjectSelection(
	std::vector<GameObject*> gameObjects,
	GameObject* primary)
{
	EGE::GameObjectSelection selection;
	selection.objects.reserve(gameObjects.size());
	for (GameObject* gameObject : gameObjects)
	{
		GameObject* valid = App && App->level
			? App->level->Validate(gameObject)
			: nullptr;
		if (!valid ||
			std::find(
				selection.objects.begin(),
				selection.objects.end(),
				valid) != selection.objects.end())
		{
			continue;
		}
		selection.objects.push_back(valid);
	}

	selection.primary = App && App->level
		? App->level->Validate(primary)
		: nullptr;
	if (std::find(
			selection.objects.begin(),
			selection.objects.end(),
			selection.primary) == selection.objects.end())
	{
		selection.primary = selection.objects.empty()
			? nullptr
			: selection.objects.back();
	}

	selected = std::move(selection);
	NotifySelectionChanged();
}

void ModuleEditor::ToggleGameObjectSelection(GameObject* gameObject)
{
	gameObject = App && App->level
		? App->level->Validate(gameObject)
		: nullptr;
	if (!gameObject)
		return;

	std::vector<GameObject*> objects;
	if (const EGE::GameObjectSelection* selection =
			GetGameObjectSelection())
	{
		objects = selection->objects;
	}

	const auto found =
		std::find(objects.begin(), objects.end(), gameObject);
	if (found == objects.end())
	{
		objects.push_back(gameObject);
		SetGameObjectSelection(std::move(objects), gameObject);
	}
	else
	{
		objects.erase(found);
		SetGameObjectSelection(std::move(objects), nullptr);
	}
}

void ModuleEditor::ClearSelected()
{
	selected = EGE::GameObjectSelection{};
	NotifySelectionChanged();
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
	EGE::EditorDialog::Open("Load File");
	if (EGE::EditorDialog::Begin(
			"Load File",
			ImVec2(520.0f, 0.0f),
			ImGuiWindowFlags_AlwaysAutoResize))
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
		if (ImGui::Button(
				"Ok", ImVec2(50.0f, ImGui::GetFrameHeight())))
			file_dialog = ready_to_close;
		ImGui::SameLine();

		if (ImGui::Button(
				"Cancel", ImVec2(50.0f, ImGui::GetFrameHeight())))
		{
			file_dialog = ready_to_close;
			selected_file[0] = '\0';
		}

		EGE::EditorDialog::End();
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
