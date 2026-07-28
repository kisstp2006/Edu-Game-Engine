#pragma once

#include "AssetImportOptions.h"
#include "ImportSettings.h"

#include <string>

class ImportAnimationDlg final
{
public:
	ImportAnimationDlg();

	void Open(const std::string& file, const std::string& name);
	void Display();
	void ClearSelection();

	[[nodiscard]] bool HasSelection() const;
	[[nodiscard]] const std::string& GetFile() const;
	[[nodiscard]] EGE::AnimationImportOptions GetOptions() const;

private:
	static constexpr const char* ClipListType =
		"EGE.AnimationClipImportList";

	std::string file_;
	EGE::ImportSettings settings_;
	bool selection_ = false;
	bool openRequested_ = false;
};
