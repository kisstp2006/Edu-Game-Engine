#ifndef EGE_SETTINGS_STORE_H
#define EGE_SETTINGS_STORE_H

#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace EGE
{
	enum class SettingType
	{
		Boolean,
		Integer,
		Number,
		String,
		Enumeration
	};

	using SettingValue = std::variant<bool, int, double, std::string>;

	struct SettingOption
	{
		std::string value;
		std::string label;
	};

	struct SettingDefinition
	{
		std::string id;
		std::string label;
		std::string description;
		SettingType type = SettingType::String;
		SettingValue defaultValue = std::string();
		double minimum = 0.0;
		double maximum = 0.0;
		double step = 1.0;
		bool hasRange = false;
		bool restartRequired = false;
		std::vector<SettingOption> options;
	};

	struct SettingCategory
	{
		std::string id;
		std::string label;
		std::vector<SettingDefinition> settings;
	};

	class SettingsStore
	{
	public:
		bool Load(
			const std::filesystem::path& schemaPath,
			const std::filesystem::path& valuesPath,
			std::string& error);
		bool ReloadValues(std::string& error);
		bool Save(std::string& error);
		void ResetToDefaults();

		const std::string& GetTitle() const;
		const std::vector<SettingCategory>& GetCategories() const;
		const std::filesystem::path& GetValuesPath() const;
		bool IsDirty() const;

		const SettingValue* FindValue(const std::string& id) const;
		bool SetValue(const std::string& id, SettingValue value);

		bool GetBool(const std::string& id, bool fallback) const;
		int GetInt(const std::string& id, int fallback) const;
		double GetNumber(const std::string& id, double fallback) const;
		std::string GetString(
			const std::string& id, const std::string& fallback) const;

	private:
		const SettingDefinition* FindDefinition(
			const std::string& id) const;
		bool LoadSchema(
			const std::filesystem::path& path, std::string& error);
		bool LoadValues(std::string& error);
		bool ValidateValue(
			const SettingDefinition& definition,
			SettingValue& value) const;

		std::string title_;
		std::vector<SettingCategory> categories_;
		std::vector<std::pair<std::string, SettingValue>> values_;
		std::filesystem::path schemaPath_;
		std::filesystem::path valuesPath_;
		bool dirty_ = false;
	};
}

#endif
