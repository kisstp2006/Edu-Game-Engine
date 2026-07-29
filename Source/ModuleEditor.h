#ifndef __MODULEEDITOR_H__
#define __MODULEEDITOR_H__

#include "EditorAssetSelection.h"
#include "EditorHistory.h"
#include "Module.h"
#include <imgui.h>
#include "imgui-filebrowser/imfilebrowser.h"
#include <filesystem>
#include <type_traits>
#include <vector>
#include <variant>
#include <memory>

#define FILE_MAX 250

union SDL_Event;

class Panel;
class PanelConsole;
class PanelGOTree;
class PanelProperties;
class PanelConfiguration;
class PanelAssets;
class PanelQuickBar;
class GameObject;
class DirLight;
class AmbientLight;
class PointLight;
class SpotLight;
class QuadLight;
class SphereLight;
class TubeLight;
class IBLData;
class LocalIBLLight;
class ComponentMeshRenderer;
namespace EGE
{
	class AssetEditorManager;
	class Project;
	class RecentProjects;

	struct GameObjectSelection
	{
		std::vector<GameObject*> objects;
		GameObject* primary = nullptr;
	};
}

class ModuleEditor : public Module
{
public:

	PanelConsole* console = nullptr;
	PanelGOTree* tree = nullptr;
	PanelProperties* props = nullptr;
	PanelConfiguration* conf = nullptr;
	PanelAssets* assets = nullptr;

    enum SelectionType
    {
        SelGameObject = 0,
        SelDirLight,
        SelAmbientLight,
        SelPointLight,
        SelSpotLight,
        SelSkybox
    };

    typedef std::variant<EGE::GameObjectSelection, ComponentMeshRenderer*, DirLight*, PointLight*, SpotLight*, QuadLight*, SphereLight*, TubeLight*, LocalIBLLight*, IBLData*, EGE::EditorAssetSelection> SelectionVariant;


    enum TabPanelEnum
    {
        TabPanelLeft = 0,
        TabPanelRight ,
        TabPanelBottom ,
        TabPanelCount
    };

public:
	ModuleEditor(bool start_enabled = true);
	~ModuleEditor();

	bool Init(Config* config = nullptr) override;
	bool Start(Config* config = nullptr) override;
	update_status PreUpdate(float dt) override;
	update_status Update(float dt) override;
	bool CleanUp() override;
	void ReceiveEvent(const Event& event) override;
	void DrawDebug() override;
	virtual void Save(Config* config) const override;

	// TODO Save/load panel activation

	void OnResize(int width, int height);

	void HandleInput(SDL_Event* event);

	bool FileDialog(const char* extension = nullptr, const char* from_folder = nullptr);
	const char* CloseFileDialog();

	void Draw();
	bool UsingMouse() const;
	bool UsingKeyboard() const;
	void Log(const char* entry);
	void LogInputEvent(uint key, uint state);
	void LogFPS(float fps, float ms);
	void PrepareForProjectChange();
	void SetProjectStatus(bool success, const std::string& message);
	void RecordRecentProject(const EGE::Project& project);
	void ApplyAppearance(const std::string& theme, bool compact);
	bool OpenAssetEditor(const EGE::EditorAssetSelection& asset);
	void CloseAssetEditors();
	bool OpenSceneAsset(const std::filesystem::path& scenePath);
	void RequestOpenScene();
	void RequestSaveScene(bool saveAs = false);
	bool BeginSceneTransaction(const std::string& label);
	void EndSceneTransaction();
	void CancelSceneTransaction();
	void SynchronizeSceneHistory();
	void ResetSceneHistory();
	bool Undo();
	bool Redo();
	[[nodiscard]] bool CanUndo() const;
	[[nodiscard]] bool CanRedo() const;
	[[nodiscard]] const char* GetUndoLabel() const;
	[[nodiscard]] const char* GetRedoLabel() const;

    int GetWidth(TabPanelEnum panel) const { return tab_panels[panel].width; }
    int GetHeight(TabPanelEnum panel) const { return tab_panels[panel].height; }
    int GetPosX(TabPanelEnum panel) const { return tab_panels[panel].posx; }
    int GetPosY(TabPanelEnum panel) const { return tab_panels[panel].posy; }

    const SelectionVariant& GetSelection() const { return selected; }
	const EGE::GameObjectSelection* GetGameObjectSelection() const;
	GameObject* GetPrimaryGameObject() const;
	bool IsGameObjectSelected(const GameObject* gameObject) const;
	void SetSelected(GameObject* gameObject);
	void SetGameObjectSelection(
		std::vector<GameObject*> gameObjects,
		GameObject* primary = nullptr);
	void ToggleGameObjectSelection(GameObject* gameObject);

    template<typename Arg>
		requires (
			!std::is_same_v<std::remove_cvref_t<Arg>, GameObject*> &&
			!std::is_same_v<
				std::remove_cvref_t<Arg>,
				EGE::GameObjectSelection>)
    void SetSelected(Arg && arg)
	{
		selected = std::forward<Arg>(arg);
		NotifySelectionChanged();
	}
    void ClearSelected();

private:

	void LoadFile(const char* filter_extension = nullptr, const char* from_dir = nullptr);
	void DrawDirectoryRecursive(const char* directory, const char* filter_extension) ;
	void DrawProjectDialogs();
	bool DrawProjectSelector();
	bool RequestProjectFromSelector(
		const std::filesystem::path& projectFile);
	void DrawSettingsWindow(bool& open, bool editorSettings);
	void HandleEditorShortcuts();
	void DrawPanelGroup(TabPanelEnum group);
	void DrawStandalonePanels(TabPanelEnum group);
	void BuildDefaultDockLayout(
		ImGuiID dockspaceId,
		const ImVec2& dockspaceSize);
	void OpenActiveProjectInVsCode();
	void NotifySelectionChanged();
	void DrawSceneDialogs();
	bool SaveSceneTo(const std::filesystem::path& selectedPath);
	std::filesystem::path GetSceneDialogDirectory() const;
	bool CaptureEditorDocumentState(
		EGE::EditorDocumentState& state) const;
	bool ApplyEditorDocumentState(
		const EGE::EditorDocumentState& state);
	void AcceptCurrentSceneHistoryState();

private:

    struct TabPanel
    {
        int width  = 0;
        int height = 0; 
        int posx   = 0; 
        int posy   = 0;
        const char* name = nullptr;

        std::vector<Panel *> panels;
    };

    TabPanel tab_panels[TabPanelCount];

	enum
	{
		closed,
		opened,
		ready_to_close
	} file_dialog = closed;
	std::string file_dialog_filter;
	std::string file_dialog_origin;

	bool capture_mouse = false;
	bool capture_keyboard = false;
	bool in_modal = false;
	char selected_file[FILE_MAX];
	bool draw_menu = true;
	bool open_new_project_popup = false;
	bool open_project_status_popup = false;
	bool show_project_settings = false;
	bool show_editor_settings = false;
	bool show_project_selector = true;
	bool project_selection_pending = false;
	char new_project_name[128] = {};
	char new_project_location[512] = {};
	std::string imgui_ini_path;
	std::string project_status_message;
	std::string project_settings_feedback;
	std::string editor_settings_feedback;
	ImGui::FileBrowser open_project_dialog;
	ImGui::FileBrowser project_location_dialog{
		ImGuiFileBrowserFlags_SelectDirectory |
		ImGuiFileBrowserFlags_CreateNewDir};
	ImGui::FileBrowser open_scene_dialog;
	ImGui::FileBrowser save_scene_dialog{
		ImGuiFileBrowserFlags_EnterNewFilename |
		ImGuiFileBrowserFlags_CreateNewDir};

    SelectionVariant selected;
	std::unique_ptr<EGE::AssetEditorManager> assetEditorManager;
	std::unique_ptr<EGE::RecentProjects> recentProjects;
	EGE::EditorHistory sceneHistory;
	EGE::EditorDocumentState historyBaseline;
	bool historyBaselineValid = false;
	bool historyTransactionEndRequested = false;
	bool historySuspended = false;

};

#endif // __MODULEEDITOR_H__
