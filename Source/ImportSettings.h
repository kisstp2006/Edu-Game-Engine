#pragma once

#include "Globals.h"
#include "Math.h"

#ifdef CreateDirectory
#undef CreateDirectory
#endif

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace EGE
{
	class ImportSettings;

	enum class ImportSettingKind
	{
		Boolean,
		Integer,
		Float,
		String,
		Enumeration,
		Vector2,
		Vector3,
		Color,
		Custom
	};

	struct ImportEnumChoice
	{
		std::string label;
		std::int64_t value = 0;
	};

	struct ImportCustomValue
	{
		std::string type;
		std::shared_ptr<void> storage;
	};

	using ImportSettingValue = std::variant<
		bool,
		std::int64_t,
		double,
		std::string,
		float2,
		float3,
		float4,
		ImportCustomValue>;

	struct ImportSetting
	{
		std::string key;
		std::string label;
		std::string tooltip;
		std::string group;
		ImportSettingKind kind = ImportSettingKind::String;
		ImportSettingValue value = std::string{};
		ImportSettingValue defaultValue = std::string{};
		std::optional<double> minimum;
		std::optional<double> maximum;
		double step = 0.1;
		std::vector<ImportEnumChoice> choices;
		std::function<bool(const ImportSettings&)> enabledWhen;
	};

	class ImportSettings final
	{
	public:
		void Clear();
		void Reset();

		ImportSetting& AddBoolean(
			std::string key, std::string label, bool value);
		ImportSetting& AddInteger(
			std::string key, std::string label, std::int64_t value);
		ImportSetting& AddFloat(
			std::string key, std::string label, double value);
		ImportSetting& AddString(
			std::string key, std::string label, std::string value);
		ImportSetting& AddEnumeration(
			std::string key,
			std::string label,
			std::int64_t value,
			std::vector<ImportEnumChoice> choices);
		ImportSetting& AddVector2(
			std::string key, std::string label, float2 value);
		ImportSetting& AddVector3(
			std::string key, std::string label, float3 value);
		ImportSetting& AddColor(
			std::string key, std::string label, float4 value);

		template<typename Value>
		ImportSetting& AddCustom(
			std::string key,
			std::string label,
			std::string type,
			Value value)
		{
			ImportSetting field;
			field.key = std::move(key);
			field.label = std::move(label);
			field.kind = ImportSettingKind::Custom;
			ImportCustomValue current;
			current.type = type;
			current.storage = std::make_shared<Value>(value);
			ImportCustomValue defaults;
			defaults.type = std::move(type);
			defaults.storage =
				std::make_shared<Value>(std::move(value));
			field.value = std::move(current);
			field.defaultValue = std::move(defaults);
			fields_.push_back(std::move(field));
			return fields_.back();
		}

		[[nodiscard]] bool GetBoolean(
			const std::string& key, bool fallback = false) const;
		[[nodiscard]] std::int64_t GetInteger(
			const std::string& key, std::int64_t fallback = 0) const;
		[[nodiscard]] double GetFloat(
			const std::string& key, double fallback = 0.0) const;
		[[nodiscard]] std::string GetString(
			const std::string& key,
			std::string fallback = {}) const;
		[[nodiscard]] float2 GetVector2(
			const std::string& key, float2 fallback = float2::zero) const;
		[[nodiscard]] float3 GetVector3(
			const std::string& key, float3 fallback = float3::zero) const;
		[[nodiscard]] float4 GetColor(
			const std::string& key, float4 fallback = float4::one) const;

		template<typename Value>
		[[nodiscard]] const Value* GetCustom(
			const std::string& key,
			const std::string& type) const
		{
			const ImportSetting* setting = Find(key);
			if (!setting)
				return nullptr;
			const auto* custom =
				std::get_if<ImportCustomValue>(&setting->value);
			if (!custom || custom->type != type || !custom->storage)
				return nullptr;
			return static_cast<const Value*>(custom->storage.get());
		}

		[[nodiscard]] ImportSetting* Find(const std::string& key);
		[[nodiscard]] const ImportSetting* Find(
			const std::string& key) const;
		[[nodiscard]] std::vector<ImportSetting>& GetFields();
		[[nodiscard]] const std::vector<ImportSetting>& GetFields() const;

	private:
		ImportSetting& Add(
			std::string key,
			std::string label,
			ImportSettingKind kind,
			ImportSettingValue value);

		std::vector<ImportSetting> fields_;
	};

	class ImportCustomEditorRegistry final
	{
	public:
		using Drawer = std::function<bool(
			const ImportSetting& setting, void* value)>;
		using Cloner = std::function<std::shared_ptr<void>(
			const void* value)>;

		static ImportCustomEditorRegistry& Get();
		void Register(
			std::string type, Drawer drawer, Cloner cloner);

		template<typename Value>
		void Register(
			std::string type,
			std::function<bool(const ImportSetting&, Value&)> drawer)
		{
			Register(
				std::move(type),
				[drawer = std::move(drawer)](
					const ImportSetting& setting, void* value)
				{
					return drawer(setting, *static_cast<Value*>(value));
				},
				[](const void* value)
				{
					return std::make_shared<Value>(
						*static_cast<const Value*>(value));
				});
		}

		[[nodiscard]] bool Draw(
			const ImportSetting& setting,
			ImportCustomValue& value) const;
		[[nodiscard]] ImportCustomValue Clone(
			const ImportCustomValue& value) const;

	private:
		struct Editor
		{
			Drawer drawer;
			Cloner cloner;
		};
		std::unordered_map<std::string, Editor> editors_;
	};

	bool DrawImportSettings(ImportSettings& settings);
}
