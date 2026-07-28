#include "PanelConsole.h"

#include "Application.h"
#include "ModuleFileSystem.h"
#include "ModuleScripting.h"
#include "Project/VsCodeWorkspace.h"
#include "Scripting/ScriptRuntime.h"

#include <imgui.h>
#include <SDL_scancode.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iterator>
#include <string_view>

namespace
{
	constexpr std::size_t MaximumEntries = 5000;

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

	std::string CurrentTime()
	{
		const std::time_t now =
			std::chrono::system_clock::to_time_t(
				std::chrono::system_clock::now());
		std::tm localTime{};
		localtime_s(&localTime, &now);

		char text[16] = {};
		std::strftime(text, sizeof(text), "%H:%M:%S", &localTime);
		return text;
	}

	const char* SeverityLabel(PanelConsole::Severity severity)
	{
		switch (severity)
		{
			case PanelConsole::Severity::Error: return "ERROR";
			case PanelConsole::Severity::Warning: return "WARN";
			default: return "INFO";
		}
	}

	ImVec4 SeverityColor(PanelConsole::Severity severity)
	{
		switch (severity)
		{
			case PanelConsole::Severity::Error:
				return ImVec4(0.96f, 0.34f, 0.34f, 1.0f);
			case PanelConsole::Severity::Warning:
				return ImVec4(0.96f, 0.72f, 0.27f, 1.0f);
			default:
				return ImVec4(0.38f, 0.70f, 0.96f, 1.0f);
		}
	}

	PanelConsole::Severity ToConsoleSeverity(
		EGE::ScriptDiagnosticSeverity severity)
	{
		switch (severity)
		{
			case EGE::ScriptDiagnosticSeverity::Error:
				return PanelConsole::Severity::Error;
			case EGE::ScriptDiagnosticSeverity::Warning:
				return PanelConsole::Severity::Warning;
			default:
				return PanelConsole::Severity::Information;
		}
	}

	bool MatchesSearch(
		const char* search,
		const std::string& message,
		const std::string& location = {})
	{
		if (!search || !*search)
			return true;

		const std::string needle = Lowercase(search);
		return Lowercase(message).find(needle) != std::string::npos ||
			Lowercase(location).find(needle) != std::string::npos;
	}

	bool DrawFilterButton(
		const char* label,
		bool& enabled,
		const ImVec4& color)
	{
		if (enabled)
		{
			ImGui::PushStyleColor(
				ImGuiCol_Button,
				ImVec4(color.x, color.y, color.z, 0.28f));
			ImGui::PushStyleColor(
				ImGuiCol_ButtonHovered,
				ImVec4(color.x, color.y, color.z, 0.42f));
		}
		else
		{
			const ImVec4 disabled =
				ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
			ImGui::PushStyleColor(ImGuiCol_Button, disabled);
			ImGui::PushStyleColor(
				ImGuiCol_ButtonHovered,
				ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		}

		const bool pressed = ImGui::Button(label);
		ImGui::PopStyleColor(2);
		if (pressed)
			enabled = !enabled;
		return pressed;
	}

	void ContinueControlLine(const char* nextLabel)
	{
		const float nextWidth =
			ImGui::CalcTextSize(nextLabel).x +
			ImGui::GetStyle().FramePadding.x * 2.0f;
		const float contentRight =
			ImGui::GetWindowPos().x +
			ImGui::GetWindowContentRegionMax().x;
		if (ImGui::GetItemRectMax().x +
				ImGui::GetStyle().ItemSpacing.x +
				nextWidth <=
			contentRight)
		{
			ImGui::SameLine();
		}
	}

	std::string DiagnosticLocation(
		const EGE::ScriptDiagnostic& diagnostic)
	{
		const std::string file = diagnostic.file.empty()
			? "<script>"
			: diagnostic.file.filename().generic_string();
		if (diagnostic.line <= 0)
			return file;

		return file + ":" + std::to_string(diagnostic.line) + ":" +
			std::to_string(std::max(diagnostic.column, 1));
	}

	void OpenDiagnostic(const EGE::ScriptDiagnostic& diagnostic)
	{
		if (!App || !App->fs || diagnostic.file.empty())
			return;

		const std::filesystem::path projectRoot =
			App->fs->GetProjectRoot();
		std::filesystem::path file = diagnostic.file;
		if (file.is_relative())
			file = projectRoot / file;

		std::string error;
		if (!EGE::OpenVsCode(
				projectRoot,
				file,
				std::max(diagnostic.line, 1),
				std::max(diagnostic.column, 1),
				error))
		{
			LOG("Could not open script diagnostic: %s", error.c_str());
		}
	}

	void DrawEntryRow(
		const char* id,
		PanelConsole::Severity severity,
		const char* source,
		const std::string& message,
		const std::string& metadata,
		const EGE::ScriptDiagnostic* diagnostic = nullptr)
	{
		ImGui::PushID(id);
		ImGui::TextColored(
			SeverityColor(severity),
			"%s",
			SeverityLabel(severity));
		ImGui::NextColumn();
		if (std::string_view(source) == "SCRIPT")
		{
			ImGui::TextColored(
				ImVec4(0.53f, 0.72f, 1.0f, 1.0f),
				"%s",
				source);
		}
		else
		{
			ImGui::TextDisabled("%s", source);
		}

		ImGui::NextColumn();
		ImGui::TextDisabled("%s", metadata.c_str());
		ImGui::NextColumn();

		const bool selected = ImGui::Selectable(
			message.c_str(),
			false,
			ImGuiSelectableFlags_AllowDoubleClick);
		if (selected &&
			diagnostic &&
			ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			OpenDiagnostic(*diagnostic);
		}

		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextColored(
				SeverityColor(severity),
				"%s / %s",
				SeverityLabel(severity),
				source);
			if (!metadata.empty())
				ImGui::TextDisabled("%s", metadata.c_str());
			ImGui::PushTextWrapPos(
				ImGui::GetFontSize() * 45.0f);
			ImGui::TextUnformatted(message.c_str());
			ImGui::PopTextWrapPos();
			if (diagnostic && !diagnostic->file.empty())
			{
				ImGui::Separator();
				ImGui::TextDisabled(
					"Double-click to open in VS Code");
			}
			ImGui::EndTooltip();
		}

		if (ImGui::BeginPopupContextItem("##ConsoleEntryOptions"))
		{
			if (ImGui::MenuItem("Copy message"))
				ImGui::SetClipboardText(message.c_str());
			if (diagnostic && !diagnostic->file.empty())
			{
				if (ImGui::MenuItem("Open in VS Code"))
					OpenDiagnostic(*diagnostic);
			}
			ImGui::EndPopup();
		}
		ImGui::NextColumn();
		ImGui::PopID();
	}
}

PanelConsole::PanelConsole()
	: Panel("Console")
{
	width = 658;
	height = 205;
	posx = 325;
	posy = 919;
}

void PanelConsole::Clear()
{
	std::scoped_lock lock(entriesMutex_);
	entries_.clear();
	scrollToBottom_ = false;
}

void PanelConsole::AddLog(const char* entry)
{
	if (!entry || !*entry)
		return;

	std::vector<Entry> additions;
	const std::string text(entry);
	std::size_t begin = 0;
	while (begin < text.size())
	{
		const std::size_t end = text.find('\n', begin);
		std::string line = text.substr(
			begin,
			end == std::string::npos
				? std::string::npos
				: end - begin);
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (!line.empty())
		{
			additions.push_back({
				DetectSeverity(line),
				CurrentTime(),
				line,
				IsScriptMessage(line),
				IsDiagnosticMirror(line)});
		}
		if (end == std::string::npos)
			break;
		begin = end + 1;
	}

	if (additions.empty())
		return;

	std::scoped_lock lock(entriesMutex_);
	entries_.insert(
		entries_.end(),
		std::make_move_iterator(additions.begin()),
		std::make_move_iterator(additions.end()));
	if (entries_.size() > MaximumEntries)
	{
		const std::size_t removeCount =
			entries_.size() - MaximumEntries;
		entries_.erase(
			entries_.begin(),
			entries_.begin() +
				static_cast<std::ptrdiff_t>(removeCount));
	}
	scrollToBottom_ = true;
}

PanelConsole::Severity PanelConsole::DetectSeverity(
	const std::string& message)
{
	const std::string text = Lowercase(message);
	if (text.find("error") != std::string::npos ||
		text.find("failed") != std::string::npos ||
		text.find("cannot") != std::string::npos ||
		text.find("could not") != std::string::npos ||
		text.find("exception") != std::string::npos ||
		text.find("invalid") != std::string::npos)
	{
		return Severity::Error;
	}
	if (text.find("warning") != std::string::npos ||
		text.find("warn") != std::string::npos)
	{
		return Severity::Warning;
	}
	return Severity::Information;
}

bool PanelConsole::IsScriptMessage(const std::string& message)
{
	return message.find("[AngelScript") != std::string::npos ||
		message.find("script") != std::string::npos ||
		message.find("Script") != std::string::npos;
}

bool PanelConsole::IsDiagnosticMirror(const std::string& message)
{
	if (message.find("[AngelScript]") == std::string::npos)
		return false;
	return message.find("): error:") != std::string::npos ||
		message.find("): warning:") != std::string::npos ||
		message.find("): info:") != std::string::npos ||
		message.find("Execution of ") != std::string::npos;
}

std::vector<PanelConsole::Entry> PanelConsole::CopyEntries(
	bool& scrollRequested)
{
	std::scoped_lock lock(entriesMutex_);
	scrollRequested = scrollToBottom_;
	scrollToBottom_ = false;
	return entries_;
}

void PanelConsole::Draw()
{
	bool scrollRequested = false;
	std::vector<Entry> entries =
		CopyEntries(scrollRequested);
	const std::vector<EGE::ScriptDiagnostic>* diagnostics = nullptr;
	if (App && App->scripting)
	{
		diagnostics =
			&App->scripting->GetRuntime().GetDiagnostics();
	}

	std::size_t informationCount = 0;
	std::size_t warningCount = 0;
	std::size_t errorCount = 0;
	std::size_t scriptCount = diagnostics ? diagnostics->size() : 0;
	for (const Entry& entry : entries)
	{
		if (entry.diagnosticMirror)
			continue;
		switch (entry.severity)
		{
			case Severity::Error: ++errorCount; break;
			case Severity::Warning: ++warningCount; break;
			default: ++informationCount; break;
		}
		if (entry.script)
			++scriptCount;
	}
	if (diagnostics)
	{
		for (const EGE::ScriptDiagnostic& diagnostic : *diagnostics)
		{
			switch (diagnostic.severity)
			{
				case EGE::ScriptDiagnosticSeverity::Error:
					++errorCount;
					break;
				case EGE::ScriptDiagnosticSeverity::Warning:
					++warningCount;
					break;
				default:
					++informationCount;
					break;
			}
		}
	}

	const bool consoleFocused =
		ImGui::IsWindowFocused(
			ImGuiFocusedFlags_RootAndChildWindows);
	const ImGuiIO& io = ImGui::GetIO();
	const bool clearShortcut =
		consoleFocused &&
		io.KeyCtrl &&
		!io.WantTextInput &&
		ImGui::IsKeyPressed(SDL_SCANCODE_L, false);
	if (ImGui::Button("Clear") || clearShortcut)
	{
		Clear();
		entries.clear();
		if (App && App->scripting)
			App->scripting->GetRuntime().ClearDiagnostics();
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Clear Console (Ctrl+L)");
	ContinueControlLine("Reload Scripts");
	const bool canReload =
		App &&
		App->scripting &&
		!App->scripting->GetRuntime().GetScriptRoot().empty();
	if (!canReload)
	{
		ImGui::PushStyleVar(
			ImGuiStyleVar_Alpha,
			ImGui::GetStyle().Alpha * 0.45f);
	}
	const bool reloadPressed = ImGui::Button("Reload Scripts");
	if (!canReload)
		ImGui::PopStyleVar();
	if (reloadPressed && canReload)
	{
		App->scripting->Reload();
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip(
			canReload
				? "Compile and reload project scripts"
				: "No active script project");
	}
	ContinueControlLine("Copy All");
	const bool hasClipboardContent =
		!entries.empty() || (diagnostics && !diagnostics->empty());
	if (!hasClipboardContent)
	{
		ImGui::PushStyleVar(
			ImGuiStyleVar_Alpha,
			ImGui::GetStyle().Alpha * 0.45f);
	}
	const bool copyAllPressed = ImGui::Button("Copy All");
	if (!hasClipboardContent)
		ImGui::PopStyleVar();
	if (copyAllPressed && hasClipboardContent)
	{
		std::string text;
		for (const Entry& entry : entries)
		{
			if (entry.diagnosticMirror)
				continue;
			text += '[' + entry.timestamp + "] [";
			text += entry.script ? "SCRIPT" : "ENGINE";
			text += "] " + entry.message + '\n';
		}
		if (diagnostics)
		{
			for (const EGE::ScriptDiagnostic& diagnostic : *diagnostics)
			{
				text += "[SCRIPT] ";
				text += DiagnosticLocation(diagnostic);
				text += " ";
				text += diagnostic.message;
				text += '\n';
			}
		}
		ImGui::SetClipboardText(text.c_str());
	}
	ContinueControlLine("Auto-scroll");
	ImGui::Checkbox("Auto-scroll", &autoScroll_);

	const bool focusSearch =
		consoleFocused &&
		io.KeyCtrl &&
		!io.WantTextInput &&
		ImGui::IsKeyPressed(SDL_SCANCODE_F, false);
	const float clearSearchWidth =
		ImGui::CalcTextSize("X").x +
		ImGui::GetStyle().FramePadding.x * 2.0f;
	ImGui::Spacing();
	if (focusSearch)
		ImGui::SetKeyboardFocusHere();
	ImGui::SetNextItemWidth(
		std::max(
			1.0f,
			ImGui::GetContentRegionAvail().x -
				clearSearchWidth -
				ImGui::GetStyle().ItemSpacing.x));
	ImGui::InputTextWithHint(
		"##ConsoleSearch",
		"Filter messages... (Ctrl+F)",
		search_,
		sizeof(search_));
	ImGui::SameLine();
	if (ImGui::Button("X##ClearConsoleSearch") && search_[0] != '\0')
		search_[0] = '\0';
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("Clear search");

	const std::string informationLabel =
		"Info " + std::to_string(informationCount);
	const std::string warningLabel =
		"Warnings " + std::to_string(warningCount);
	const std::string errorLabel =
		"Errors " + std::to_string(errorCount);
	const std::string scriptLabel =
		"Scripts " + std::to_string(scriptCount);

	DrawFilterButton(
		informationLabel.c_str(),
		showInformation_,
		SeverityColor(Severity::Information));
	ContinueControlLine(warningLabel.c_str());
	DrawFilterButton(
		warningLabel.c_str(),
		showWarnings_,
		SeverityColor(Severity::Warning));
	ContinueControlLine(errorLabel.c_str());
	DrawFilterButton(
		errorLabel.c_str(),
		showErrors_,
		SeverityColor(Severity::Error));
	ContinueControlLine(scriptLabel.c_str());
	DrawFilterButton(
		scriptLabel.c_str(),
		showScriptMessages_,
		ImVec4(0.53f, 0.72f, 1.0f, 1.0f));

	ImGui::Separator();
	if (ImGui::BeginChild(
			"##ConsoleMessages",
			ImVec2(0.0f, 0.0f),
			false,
			ImGuiWindowFlags_HorizontalScrollbar))
	{
		const float availableWidth =
			std::max(360.0f, ImGui::GetContentRegionAvail().x);
		ImGui::Columns(4, "##ConsoleColumns", false);
		ImGui::SetColumnWidth(0, 64.0f);
		ImGui::SetColumnWidth(1, 70.0f);
		ImGui::SetColumnWidth(
			2,
			std::clamp(availableWidth * 0.22f, 100.0f, 190.0f));
		ImGui::TextDisabled("LEVEL");
		ImGui::NextColumn();
		ImGui::TextDisabled("SOURCE");
		ImGui::NextColumn();
		ImGui::TextDisabled("TIME / LOCATION");
		ImGui::NextColumn();
		ImGui::TextDisabled("MESSAGE");
		ImGui::NextColumn();
		ImGui::Separator();

		std::size_t visibleIndex = 0;
		for (const Entry& entry : entries)
		{
			if (entry.diagnosticMirror ||
				(entry.script && !showScriptMessages_) ||
				(entry.severity == Severity::Information &&
					!showInformation_) ||
				(entry.severity == Severity::Warning &&
					!showWarnings_) ||
				(entry.severity == Severity::Error &&
					!showErrors_) ||
				!MatchesSearch(search_, entry.message))
			{
				continue;
			}

			const std::string id =
				"log_" + std::to_string(visibleIndex++);
			DrawEntryRow(
				id.c_str(),
				entry.severity,
				entry.script ? "SCRIPT" : "ENGINE",
				entry.message,
				entry.timestamp);
		}

		if (diagnostics && showScriptMessages_)
		{
			for (std::size_t index = 0;
				index < diagnostics->size();
				++index)
			{
				const EGE::ScriptDiagnostic& diagnostic =
					(*diagnostics)[index];
				const Severity severity =
					ToConsoleSeverity(diagnostic.severity);
				if ((severity == Severity::Information &&
						!showInformation_) ||
					(severity == Severity::Warning &&
						!showWarnings_) ||
					(severity == Severity::Error &&
						!showErrors_))
				{
					continue;
				}

				const std::string location =
					DiagnosticLocation(diagnostic);
				if (!MatchesSearch(
						search_,
						diagnostic.message,
						location))
				{
					continue;
				}

				const std::string id =
					"diagnostic_" + std::to_string(index);
				DrawEntryRow(
					id.c_str(),
					severity,
					"SCRIPT",
					diagnostic.message,
					location,
					&diagnostic);
			}
		}

		if (autoScroll_ && scrollRequested)
			ImGui::SetScrollHereY(1.0f);
		ImGui::Columns(1);
	}
	ImGui::EndChild();
}
