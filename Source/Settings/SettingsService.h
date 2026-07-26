#ifndef EGE_SETTINGS_SERVICE_H
#define EGE_SETTINGS_SERVICE_H

#include "SettingsStore.h"

#include <filesystem>
#include <string>

namespace EGE
{
	class SettingsService
	{
	public:
		bool Initialize(
			const std::filesystem::path& fallbackRoot,
			const std::filesystem::path& projectRoot,
			bool loadEditorSettings,
			std::string& error);
		bool ChangeProject(
			const std::filesystem::path& projectRoot,
			std::string& error);
		bool SaveAll(std::string& error);

		SettingsStore& Project();
		const SettingsStore& Project() const;
		SettingsStore& Editor();
		const SettingsStore& Editor() const;
		bool HasEditorSettings() const;

	private:
		std::filesystem::path FindSchema(
			const std::filesystem::path& projectRoot,
			const char* fileName) const;
		bool LoadProject(
			const std::filesystem::path& projectRoot,
			std::string& error);

		std::filesystem::path fallbackRoot_;
		SettingsStore project_;
		SettingsStore editor_;
		bool hasEditorSettings_ = false;
	};
}

#endif
