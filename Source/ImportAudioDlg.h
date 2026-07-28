#pragma once

#include "AssetImportOptions.h"
#include "ImportSettings.h"

#include <string>

class ImportAudioDlg final
{
public:
	ImportAudioDlg();

	void Open(const std::string& file);
	void Display();
	void ClearSelection();

	[[nodiscard]] bool HasSelection() const;
	[[nodiscard]] const std::string& GetFile() const;
	[[nodiscard]] EGE::AudioImportOptions GetOptions() const;

private:
	std::string file_;
	std::string popupName_;
	EGE::ImportSettings settings_;
	bool selection_ = false;
	bool openRequested_ = false;
};
