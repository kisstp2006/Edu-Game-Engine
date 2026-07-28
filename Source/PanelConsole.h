#ifndef __PANELCONSOLE_H__
#define __PANELCONSOLE_H__

#include "Panel.h"

#include <mutex>
#include <string>
#include <vector>

class PanelConsole final : public Panel
{
public:
	enum class Severity
	{
		Information,
		Warning,
		Error
	};

	PanelConsole();

	void Draw() override;
	void Clear();
	void AddLog(const char* entry);

private:
	struct Entry
	{
		Severity severity = Severity::Information;
		std::string timestamp;
		std::string message;
		bool script = false;
		bool diagnosticMirror = false;
	};

	static Severity DetectSeverity(const std::string& message);
	static bool IsScriptMessage(const std::string& message);
	static bool IsDiagnosticMirror(const std::string& message);

	std::vector<Entry> CopyEntries(bool& scrollRequested);

	std::mutex entriesMutex_;
	std::vector<Entry> entries_;
	bool scrollToBottom_ = false;
	bool autoScroll_ = true;
	bool showInformation_ = true;
	bool showWarnings_ = true;
	bool showErrors_ = true;
	bool showScriptMessages_ = true;
	char search_[256] = {};
};

#endif
