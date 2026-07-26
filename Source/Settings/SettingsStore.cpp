#include "SettingsStore.h"

#include "../parson.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#include <Windows.h>
#endif

namespace EGE
{
	namespace
	{
		using JsonValuePtr =
			std::unique_ptr<JSON_Value, decltype(&json_value_free)>;
		using JsonStringPtr =
			std::unique_ptr<char, decltype(&json_free_serialized_string)>;

		constexpr int SettingsFormatVersion = 1;

		bool ReadFile(
			const std::filesystem::path& path, std::string& contents)
		{
			std::ifstream input(path, std::ios::binary);
			if (!input.is_open())
				return false;

			std::ostringstream stream;
			stream << input.rdbuf();
			if (!input.good() && !input.eof())
				return false;

			contents = stream.str();
			return true;
		}

		bool ReplaceFile(
			const std::filesystem::path& temporary,
			const std::filesystem::path& destination,
			std::string& error)
		{
#if defined(_WIN32)
			if (MoveFileExW(
					temporary.c_str(), destination.c_str(),
					MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
			{
				return true;
			}

			const std::error_code moveError(
				static_cast<int>(GetLastError()), std::system_category());
#else
			std::error_code moveError;
			std::filesystem::rename(temporary, destination, moveError);
			if (!moveError)
				return true;
#endif
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			error = "Could not replace settings file '" +
				destination.string() + "': " + moveError.message();
			return false;
		}

		bool ReadString(
			const JSON_Object* object, const char* field,
			std::string& output, bool required)
		{
			const JSON_Value* value = json_object_get_value(object, field);
			if (!value)
				return !required;
			if (json_value_get_type(value) != JSONString)
				return false;

			output = json_value_get_string(value);
			return true;
		}

		bool ReadBool(
			const JSON_Object* object, const char* field, bool fallback)
		{
			const JSON_Value* value = json_object_get_value(object, field);
			return value && json_value_get_type(value) == JSONBoolean
				? json_value_get_boolean(value) != 0
				: fallback;
		}

		bool ReadNumber(
			const JSON_Object* object, const char* field, double& output)
		{
			const JSON_Value* value = json_object_get_value(object, field);
			if (!value || json_value_get_type(value) != JSONNumber)
				return false;
			output = json_value_get_number(value);
			return std::isfinite(output);
		}

		bool ParseType(const std::string& text, SettingType& type)
		{
			if (text == "boolean")
				type = SettingType::Boolean;
			else if (text == "integer")
				type = SettingType::Integer;
			else if (text == "number")
				type = SettingType::Number;
			else if (text == "string")
				type = SettingType::String;
			else if (text == "enum")
				type = SettingType::Enumeration;
			else
				return false;
			return true;
		}

		bool ParseDefault(
			const JSON_Object* object, SettingDefinition& definition)
		{
			const JSON_Value* value =
				json_object_get_value(object, "Default");
			if (!value)
				return false;

			switch (definition.type)
			{
			case SettingType::Boolean:
				if (json_value_get_type(value) != JSONBoolean)
					return false;
				definition.defaultValue =
					json_value_get_boolean(value) != 0;
				return true;

			case SettingType::Integer:
			{
				if (json_value_get_type(value) != JSONNumber)
					return false;
				const double number = json_value_get_number(value);
				if (!std::isfinite(number) || std::floor(number) != number)
					return false;
				definition.defaultValue = static_cast<int>(number);
				return true;
			}

			case SettingType::Number:
				if (json_value_get_type(value) != JSONNumber ||
					!std::isfinite(json_value_get_number(value)))
				{
					return false;
				}
				definition.defaultValue = json_value_get_number(value);
				return true;

			case SettingType::String:
			case SettingType::Enumeration:
				if (json_value_get_type(value) != JSONString)
					return false;
				definition.defaultValue =
					std::string(json_value_get_string(value));
				return true;
			}
			return false;
		}
	}

	bool SettingsStore::Load(
		const std::filesystem::path& schemaPath,
		const std::filesystem::path& valuesPath,
		std::string& error)
	{
		schemaPath_ = schemaPath;
		valuesPath_ = valuesPath;
		if (!LoadSchema(schemaPath_, error))
			return false;

		ResetToDefaults();
		if (!LoadValues(error))
			return false;

		if (!std::filesystem::exists(valuesPath_))
			return Save(error);

		dirty_ = false;
		return true;
	}

	bool SettingsStore::LoadSchema(
		const std::filesystem::path& path, std::string& error)
	{
		std::string contents;
		if (!ReadFile(path, contents))
		{
			error = "Could not read settings schema '" + path.string() + "'.";
			return false;
		}

		JsonValuePtr rootValue(
			json_parse_string(contents.c_str()), json_value_free);
		if (!rootValue ||
			json_value_get_type(rootValue.get()) != JSONObject)
		{
			error = "Settings schema is not a valid JSON object: " +
				path.string();
			return false;
		}

		const JSON_Object* root = json_value_get_object(rootValue.get());
		const double version = json_object_get_number(root, "FormatVersion");
		if (version != SettingsFormatVersion)
		{
			error = "Unsupported settings schema version in '" +
				path.string() + "'.";
			return false;
		}

		std::string title;
		if (!ReadString(root, "Title", title, true))
		{
			error = "Settings schema has no valid Title.";
			return false;
		}

		const JSON_Array* categoryArray =
			json_object_get_array(root, "Categories");
		if (!categoryArray)
		{
			error = "Settings schema has no Categories array.";
			return false;
		}

		std::vector<SettingCategory> categories;
		std::vector<std::string> ids;
		for (size_t categoryIndex = 0;
			categoryIndex < json_array_get_count(categoryArray);
			++categoryIndex)
		{
			const JSON_Object* categoryObject =
				json_array_get_object(categoryArray, categoryIndex);
			SettingCategory category;
			if (!categoryObject ||
				!ReadString(categoryObject, "Id", category.id, true) ||
				!ReadString(categoryObject, "Label", category.label, true))
			{
				error = "A settings category has an invalid Id or Label.";
				return false;
			}

			const JSON_Array* settingsArray =
				json_object_get_array(categoryObject, "Settings");
			if (!settingsArray)
			{
				error = "Settings category '" + category.id +
					"' has no Settings array.";
				return false;
			}

			for (size_t settingIndex = 0;
				settingIndex < json_array_get_count(settingsArray);
				++settingIndex)
			{
				const JSON_Object* settingObject =
					json_array_get_object(settingsArray, settingIndex);
				SettingDefinition definition;
				std::string type;
				if (!settingObject ||
					!ReadString(settingObject, "Id", definition.id, true) ||
					!ReadString(
						settingObject, "Label", definition.label, true) ||
					!ReadString(
						settingObject, "Description",
						definition.description, false) ||
					!ReadString(settingObject, "Type", type, true) ||
					!ParseType(type, definition.type) ||
					!ParseDefault(settingObject, definition))
				{
					error = "A setting in category '" + category.id +
						"' has an invalid definition.";
					return false;
				}

				if (definition.id.empty() ||
					std::find(ids.begin(), ids.end(), definition.id) != ids.end())
				{
					error = "Duplicate or empty setting id '" +
						definition.id + "'.";
					return false;
				}
				ids.push_back(definition.id);

				double minimum = 0.0;
				double maximum = 0.0;
				if (ReadNumber(settingObject, "Min", minimum) &&
					ReadNumber(settingObject, "Max", maximum) &&
					minimum <= maximum)
				{
					definition.minimum = minimum;
					definition.maximum = maximum;
					definition.hasRange = true;
				}
				ReadNumber(settingObject, "Step", definition.step);
				if (definition.step <= 0.0)
					definition.step = 1.0;
				definition.restartRequired = ReadBool(
					settingObject, "RestartRequired", false);

				if (definition.type == SettingType::Enumeration)
				{
					const JSON_Array* optionArray =
						json_object_get_array(settingObject, "Options");
					if (!optionArray)
					{
						error = "Enum setting '" + definition.id +
							"' has no Options array.";
						return false;
					}

					for (size_t optionIndex = 0;
						optionIndex < json_array_get_count(optionArray);
						++optionIndex)
					{
						const JSON_Object* optionObject =
							json_array_get_object(optionArray, optionIndex);
						SettingOption option;
						if (!optionObject ||
							!ReadString(
								optionObject, "Value", option.value, true) ||
							!ReadString(
								optionObject, "Label", option.label, true))
						{
							error = "Enum setting '" + definition.id +
								"' contains an invalid option.";
							return false;
						}
						definition.options.push_back(std::move(option));
					}
				}

				SettingValue validatedDefault = definition.defaultValue;
				if (!ValidateValue(definition, validatedDefault))
				{
					error = "Setting '" + definition.id +
						"' has an invalid default value.";
					return false;
				}
				definition.defaultValue = std::move(validatedDefault);
				category.settings.push_back(std::move(definition));
			}
			categories.push_back(std::move(category));
		}

		title_ = std::move(title);
		categories_ = std::move(categories);
		return true;
	}

	bool SettingsStore::LoadValues(std::string& error)
	{
		if (!std::filesystem::exists(valuesPath_))
			return true;

		std::string contents;
		if (!ReadFile(valuesPath_, contents))
		{
			error = "Could not read settings values '" +
				valuesPath_.string() + "'.";
			return false;
		}

		JsonValuePtr rootValue(
			json_parse_string(contents.c_str()), json_value_free);
		if (!rootValue ||
			json_value_get_type(rootValue.get()) != JSONObject)
		{
			error = "Settings values are not valid JSON: " +
				valuesPath_.string();
			return false;
		}

		const JSON_Object* root = json_value_get_object(rootValue.get());
		if (json_object_get_number(root, "FormatVersion") !=
			SettingsFormatVersion)
		{
			error = "Unsupported settings values version in '" +
				valuesPath_.string() + "'.";
			return false;
		}

		const JSON_Object* valuesObject =
			json_object_get_object(root, "Values");
		if (!valuesObject)
		{
			error = "Settings values file has no Values object.";
			return false;
		}

		for (const SettingCategory& category : categories_)
		{
			for (const SettingDefinition& definition : category.settings)
			{
				const JSON_Value* jsonValue =
					json_object_get_value(valuesObject, definition.id.c_str());
				if (!jsonValue)
					continue;

				SettingValue value;
				switch (definition.type)
				{
				case SettingType::Boolean:
					if (json_value_get_type(jsonValue) != JSONBoolean)
						continue;
					value = json_value_get_boolean(jsonValue) != 0;
					break;
				case SettingType::Integer:
				{
					if (json_value_get_type(jsonValue) != JSONNumber)
						continue;
					const double number = json_value_get_number(jsonValue);
					if (std::floor(number) != number)
						continue;
					value = static_cast<int>(number);
					break;
				}
				case SettingType::Number:
					if (json_value_get_type(jsonValue) != JSONNumber)
						continue;
					value = json_value_get_number(jsonValue);
					break;
				case SettingType::String:
				case SettingType::Enumeration:
					if (json_value_get_type(jsonValue) != JSONString)
						continue;
					value = std::string(json_value_get_string(jsonValue));
					break;
				}

				if (ValidateValue(definition, value))
					SetValue(definition.id, std::move(value));
			}
		}
		dirty_ = false;
		return true;
	}

	bool SettingsStore::ReloadValues(std::string& error)
	{
		ResetToDefaults();
		if (!LoadValues(error))
			return false;
		dirty_ = false;
		return true;
	}

	bool SettingsStore::Save(std::string& error)
	{
		std::error_code directoryError;
		std::filesystem::create_directories(
			valuesPath_.parent_path(), directoryError);
		if (directoryError)
		{
			error = "Could not create settings directory: " +
				directoryError.message();
			return false;
		}

		JsonValuePtr rootValue(json_value_init_object(), json_value_free);
		JsonValuePtr valuesValue(json_value_init_object(), json_value_free);
		if (!rootValue || !valuesValue)
		{
			error = "Could not allocate settings JSON document.";
			return false;
		}

		JSON_Object* root = json_value_get_object(rootValue.get());
		JSON_Object* valuesObject =
			json_value_get_object(valuesValue.get());
		json_object_set_number(root, "FormatVersion", SettingsFormatVersion);

		for (const auto& entry : values_)
		{
			const std::string& id = entry.first;
			const SettingValue& value = entry.second;
			if (const bool* boolean = std::get_if<bool>(&value))
				json_object_set_boolean(
					valuesObject, id.c_str(), *boolean ? 1 : 0);
			else if (const int* integer = std::get_if<int>(&value))
				json_object_set_number(valuesObject, id.c_str(), *integer);
			else if (const double* number = std::get_if<double>(&value))
				json_object_set_number(valuesObject, id.c_str(), *number);
			else if (const std::string* string =
				std::get_if<std::string>(&value))
			{
				json_object_set_string(
					valuesObject, id.c_str(), string->c_str());
			}
		}

		JSON_Value* rawValues = valuesValue.release();
		if (json_object_set_value(root, "Values", rawValues) != JSONSuccess)
		{
			json_value_free(rawValues);
			error = "Could not attach settings values to JSON document.";
			return false;
		}

		JsonStringPtr serialized(
			json_serialize_to_string_pretty(rootValue.get()),
			json_free_serialized_string);
		if (!serialized)
		{
			error = "Could not serialize settings JSON document.";
			return false;
		}

		std::filesystem::path temporary = valuesPath_;
		temporary += ".tmp-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count());
		std::ofstream output(
			temporary, std::ios::binary | std::ios::trunc);
		if (!output.is_open())
		{
			error = "Could not open temporary settings file '" +
				temporary.string() + "'.";
			return false;
		}
		output << serialized.get() << '\n';
		output.flush();
		if (!output.good())
		{
			error = "Could not write temporary settings file '" +
				temporary.string() + "'.";
			return false;
		}
		output.close();

		if (!ReplaceFile(temporary, valuesPath_, error))
			return false;

		dirty_ = false;
		return true;
	}

	void SettingsStore::ResetToDefaults()
	{
		values_.clear();
		for (const SettingCategory& category : categories_)
		{
			for (const SettingDefinition& definition : category.settings)
				values_.emplace_back(definition.id, definition.defaultValue);
		}
		dirty_ = true;
	}

	const std::string& SettingsStore::GetTitle() const
	{
		return title_;
	}

	const std::vector<SettingCategory>& SettingsStore::GetCategories() const
	{
		return categories_;
	}

	const std::filesystem::path& SettingsStore::GetValuesPath() const
	{
		return valuesPath_;
	}

	bool SettingsStore::IsDirty() const
	{
		return dirty_;
	}

	const SettingDefinition* SettingsStore::FindDefinition(
		const std::string& id) const
	{
		for (const SettingCategory& category : categories_)
		{
			for (const SettingDefinition& definition : category.settings)
			{
				if (definition.id == id)
					return &definition;
			}
		}
		return nullptr;
	}

	const SettingValue* SettingsStore::FindValue(
		const std::string& id) const
	{
		const auto found = std::find_if(
			values_.begin(), values_.end(),
			[&id](const auto& entry)
			{
				return entry.first == id;
			});
		return found == values_.end() ? nullptr : &found->second;
	}

	bool SettingsStore::SetValue(const std::string& id, SettingValue value)
	{
		const SettingDefinition* definition = FindDefinition(id);
		if (!definition || !ValidateValue(*definition, value))
			return false;

		for (auto& entry : values_)
		{
			if (entry.first == id)
			{
				if (entry.second != value)
				{
					entry.second = std::move(value);
					dirty_ = true;
				}
				return true;
			}
		}

		values_.emplace_back(id, std::move(value));
		dirty_ = true;
		return true;
	}

	bool SettingsStore::ValidateValue(
		const SettingDefinition& definition, SettingValue& value) const
	{
		switch (definition.type)
		{
		case SettingType::Boolean:
			return std::holds_alternative<bool>(value);
		case SettingType::Integer:
			if (int* integer = std::get_if<int>(&value))
			{
				if (definition.hasRange)
				*integer = std::clamp(
					*integer, static_cast<int>(definition.minimum),
					static_cast<int>(definition.maximum));
				return true;
			}
			return false;
		case SettingType::Number:
			if (double* number = std::get_if<double>(&value))
			{
				if (!std::isfinite(*number))
					return false;
				if (definition.hasRange)
					*number = std::clamp(
						*number, definition.minimum, definition.maximum);
				return true;
			}
			return false;
		case SettingType::String:
			return std::holds_alternative<std::string>(value);
		case SettingType::Enumeration:
			if (const std::string* string =
				std::get_if<std::string>(&value))
			{
				return std::any_of(
					definition.options.begin(), definition.options.end(),
					[string](const SettingOption& option)
					{
						return option.value == *string;
					});
			}
			return false;
		}
		return false;
	}

	bool SettingsStore::GetBool(
		const std::string& id, bool fallback) const
	{
		const SettingValue* value = FindValue(id);
		const bool* result = value ? std::get_if<bool>(value) : nullptr;
		return result ? *result : fallback;
	}

	int SettingsStore::GetInt(
		const std::string& id, int fallback) const
	{
		const SettingValue* value = FindValue(id);
		const int* result = value ? std::get_if<int>(value) : nullptr;
		return result ? *result : fallback;
	}

	double SettingsStore::GetNumber(
		const std::string& id, double fallback) const
	{
		const SettingValue* value = FindValue(id);
		const double* result = value ? std::get_if<double>(value) : nullptr;
		return result ? *result : fallback;
	}

	std::string SettingsStore::GetString(
		const std::string& id, const std::string& fallback) const
	{
		const SettingValue* value = FindValue(id);
		const std::string* result =
			value ? std::get_if<std::string>(value) : nullptr;
		return result ? *result : fallback;
	}
}
