#include "PanelScriptDiagnostics.h"

#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleScripting.h"
#include "Project/VsCodeWorkspace.h"

#include <imgui.h>

namespace
{
	const char* SeverityLabel(EGE::ScriptDiagnosticSeverity severity)
	{
		switch (severity)
		{
			case EGE::ScriptDiagnosticSeverity::Error: return "Error";
			case EGE::ScriptDiagnosticSeverity::Warning: return "Warning";
			default: return "Info";
		}
	}
}

PanelScriptDiagnostics::PanelScriptDiagnostics()
	: Panel("Script Diagnostics")
{
}

void PanelScriptDiagnostics::Draw()
{
	if (!App || !App->scripting)
	{
		ImGui::TextDisabled("Scripting runtime is unavailable.");
		return;
	}

	const std::vector<EGE::ScriptDiagnostic>& diagnostics =
		App->scripting->GetRuntime().GetDiagnostics();
	if (diagnostics.empty())
	{
		ImGui::TextDisabled("No script diagnostics.");
		return;
	}

	for (const EGE::ScriptDiagnostic& diagnostic : diagnostics)
	{
		const std::string location = diagnostic.file.empty()
			? "<script>"
			: diagnostic.file.generic_string();
		const std::string label = std::string(SeverityLabel(diagnostic.severity)) +
			"  " + location + ":" + std::to_string(diagnostic.line) +
			":" + std::to_string(diagnostic.column) + "\n" +
			diagnostic.message;
		if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
		{
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
				App->fs && !diagnostic.file.empty())
			{
				std::filesystem::path file = diagnostic.file;
				if (file.is_relative())
					file = App->fs->GetProjectRoot() / file;
				std::string error;
				EGE::OpenVsCode(
					App->fs->GetProjectRoot(), file, diagnostic.line,
					diagnostic.column, error);
			}
		}
	}
}
