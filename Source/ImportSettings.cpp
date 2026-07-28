#include "ImportSettings.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace EGE
{
	namespace
	{
		template<typename Value>
		Value ReadValue(
			const ImportSetting* setting,
			Value fallback)
		{
			if (!setting)
				return fallback;
			const Value* value = std::get_if<Value>(&setting->value);
			return value ? *value : fallback;
		}

		bool DrawField(
			ImportSetting& field,
			const ImportSettings& settings)
		{
			const bool enabled =
				!field.enabledWhen || field.enabledWhen(settings);
			if (!enabled)
			{
				ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
				ImGui::PushStyleVar(
					ImGuiStyleVar_Alpha,
					ImGui::GetStyle().Alpha * 0.45f);
			}

			ImGui::PushID(field.key.c_str());
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(field.label.c_str());
			if (!field.tooltip.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", field.tooltip.c_str());

			const float labelWidth = 165.0f;
			if (ImGui::CalcTextSize(field.label.c_str()).x <
				labelWidth - ImGui::GetStyle().ItemSpacing.x)
			{
				ImGui::SameLine(labelWidth);
			}
			ImGui::SetNextItemWidth(-1.0f);

			bool changed = false;
			switch (field.kind)
			{
			case ImportSettingKind::Boolean:
				changed = ImGui::Checkbox(
					"##Value", &std::get<bool>(field.value));
				break;
			case ImportSettingKind::Integer:
			{
				std::int64_t& value =
					std::get<std::int64_t>(field.value);
				if (field.minimum && field.maximum)
				{
					const std::int64_t minimum =
						static_cast<std::int64_t>(*field.minimum);
					const std::int64_t maximum =
						static_cast<std::int64_t>(*field.maximum);
					changed = ImGui::SliderScalar(
						"##Value", ImGuiDataType_S64, &value,
						&minimum, &maximum);
				}
				else
				{
					changed = ImGui::InputScalar(
						"##Value", ImGuiDataType_S64, &value);
				}
				break;
			}
			case ImportSettingKind::Float:
			{
				double& value = std::get<double>(field.value);
				const float speed =
					static_cast<float>(std::max(0.0001, field.step));
				const double minimum = field.minimum.value_or(0.0);
				const double maximum = field.maximum.value_or(0.0);
				changed = ImGui::DragScalar(
					"##Value", ImGuiDataType_Double, &value, speed,
					field.minimum ? &minimum : nullptr,
					field.maximum ? &maximum : nullptr);
				break;
			}
			case ImportSettingKind::String:
				changed = ImGui::InputText(
					"##Value", &std::get<std::string>(field.value));
				break;
			case ImportSettingKind::Enumeration:
			{
				std::int64_t& value =
					std::get<std::int64_t>(field.value);
				const char* preview = "Unknown";
				for (const ImportEnumChoice& choice : field.choices)
				{
					if (choice.value == value)
					{
						preview = choice.label.c_str();
						break;
					}
				}
				if (ImGui::BeginCombo("##Value", preview))
				{
					for (const ImportEnumChoice& choice : field.choices)
					{
						const bool selected = choice.value == value;
						if (ImGui::Selectable(
								choice.label.c_str(), selected))
						{
							value = choice.value;
							changed = true;
						}
						if (selected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				break;
			}
			case ImportSettingKind::Vector2:
			{
				float2& value = std::get<float2>(field.value);
				float components[] = {value.x, value.y};
				changed = ImGui::DragFloat2(
					"##Value", components,
					static_cast<float>(field.step),
					static_cast<float>(field.minimum.value_or(0.0)),
					static_cast<float>(field.maximum.value_or(0.0)));
				if (changed)
					value = float2(components[0], components[1]);
				break;
			}
			case ImportSettingKind::Vector3:
			{
				float3& value = std::get<float3>(field.value);
				float components[] = {value.x, value.y, value.z};
				changed = ImGui::DragFloat3(
					"##Value", components,
					static_cast<float>(field.step),
					static_cast<float>(field.minimum.value_or(0.0)),
					static_cast<float>(field.maximum.value_or(0.0)));
				if (changed)
					value = float3(
						components[0], components[1], components[2]);
				break;
			}
			case ImportSettingKind::Color:
			{
				float4& value = std::get<float4>(field.value);
				float components[] = {
					value.x, value.y, value.z, value.w};
				changed = ImGui::ColorEdit4(
					"##Value", components,
					ImGuiColorEditFlags_Float |
						ImGuiColorEditFlags_AlphaBar);
				if (changed)
					value = float4(
						components[0], components[1],
						components[2], components[3]);
				break;
			}
			case ImportSettingKind::Custom:
				changed = ImportCustomEditorRegistry::Get().Draw(
					field, std::get<ImportCustomValue>(field.value));
				break;
			}
			ImGui::PopID();

			if (!enabled)
			{
				ImGui::PopStyleVar();
				ImGui::PopItemFlag();
			}
			return changed;
		}
	}

	void ImportSettings::Clear()
	{
		fields_.clear();
	}

	void ImportSettings::Reset()
	{
		for (ImportSetting& field : fields_)
		{
			if (const auto* custom =
					std::get_if<ImportCustomValue>(&field.defaultValue))
			{
				field.value =
					ImportCustomEditorRegistry::Get().Clone(*custom);
			}
			else
			{
				field.value = field.defaultValue;
			}
		}
	}

	ImportSetting& ImportSettings::AddBoolean(
		std::string key, std::string label, bool value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::Boolean, value);
	}

	ImportSetting& ImportSettings::AddInteger(
		std::string key, std::string label, std::int64_t value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::Integer, value);
	}

	ImportSetting& ImportSettings::AddFloat(
		std::string key, std::string label, double value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::Float, value);
	}

	ImportSetting& ImportSettings::AddString(
		std::string key, std::string label, std::string value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::String, std::move(value));
	}

	ImportSetting& ImportSettings::AddEnumeration(
		std::string key,
		std::string label,
		std::int64_t value,
		std::vector<ImportEnumChoice> choices)
	{
		ImportSetting& setting = Add(
			std::move(key), std::move(label),
			ImportSettingKind::Enumeration, value);
		setting.choices = std::move(choices);
		return setting;
	}

	ImportSetting& ImportSettings::AddVector2(
		std::string key, std::string label, float2 value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::Vector2, value);
	}

	ImportSetting& ImportSettings::AddVector3(
		std::string key, std::string label, float3 value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::Vector3, value);
	}

	ImportSetting& ImportSettings::AddColor(
		std::string key, std::string label, float4 value)
	{
		return Add(
			std::move(key), std::move(label),
			ImportSettingKind::Color, value);
	}

	bool ImportSettings::GetBoolean(
		const std::string& key, bool fallback) const
	{
		return ReadValue(Find(key), fallback);
	}

	std::int64_t ImportSettings::GetInteger(
		const std::string& key, std::int64_t fallback) const
	{
		return ReadValue(Find(key), fallback);
	}

	double ImportSettings::GetFloat(
		const std::string& key, double fallback) const
	{
		return ReadValue(Find(key), fallback);
	}

	std::string ImportSettings::GetString(
		const std::string& key, std::string fallback) const
	{
		return ReadValue(Find(key), std::move(fallback));
	}

	float2 ImportSettings::GetVector2(
		const std::string& key, float2 fallback) const
	{
		return ReadValue(Find(key), fallback);
	}

	float3 ImportSettings::GetVector3(
		const std::string& key, float3 fallback) const
	{
		return ReadValue(Find(key), fallback);
	}

	float4 ImportSettings::GetColor(
		const std::string& key, float4 fallback) const
	{
		return ReadValue(Find(key), fallback);
	}

	ImportSetting* ImportSettings::Find(const std::string& key)
	{
		const auto found = std::find_if(
			fields_.begin(), fields_.end(),
			[&key](const ImportSetting& field)
			{
				return field.key == key;
			});
		return found == fields_.end() ? nullptr : &*found;
	}

	const ImportSetting* ImportSettings::Find(
		const std::string& key) const
	{
		const auto found = std::find_if(
			fields_.begin(), fields_.end(),
			[&key](const ImportSetting& field)
			{
				return field.key == key;
			});
		return found == fields_.end() ? nullptr : &*found;
	}

	std::vector<ImportSetting>& ImportSettings::GetFields()
	{
		return fields_;
	}

	const std::vector<ImportSetting>& ImportSettings::GetFields() const
	{
		return fields_;
	}

	ImportSetting& ImportSettings::Add(
		std::string key,
		std::string label,
		ImportSettingKind kind,
		ImportSettingValue value)
	{
		ImportSetting field;
		field.key = std::move(key);
		field.label = std::move(label);
		field.kind = kind;
		field.value = value;
		field.defaultValue = std::move(value);
		fields_.push_back(std::move(field));
		return fields_.back();
	}

	ImportCustomEditorRegistry& ImportCustomEditorRegistry::Get()
	{
		static ImportCustomEditorRegistry registry;
		return registry;
	}

	void ImportCustomEditorRegistry::Register(
		std::string type, Drawer drawer, Cloner cloner)
	{
		editors_.insert_or_assign(
			std::move(type),
			Editor{std::move(drawer), std::move(cloner)});
	}

	bool ImportCustomEditorRegistry::Draw(
		const ImportSetting& setting,
		ImportCustomValue& value) const
	{
		const auto found = editors_.find(value.type);
		if (found == editors_.end() || !value.storage)
		{
			ImGui::TextDisabled(
				"No editor registered for %s", value.type.c_str());
			return false;
		}
		return found->second.drawer(setting, value.storage.get());
	}

	ImportCustomValue ImportCustomEditorRegistry::Clone(
		const ImportCustomValue& value) const
	{
		ImportCustomValue result;
		result.type = value.type;
		const auto found = editors_.find(value.type);
		if (found != editors_.end() && found->second.cloner &&
			value.storage)
		{
			result.storage = found->second.cloner(value.storage.get());
		}
		else
		{
			result.storage = value.storage;
		}
		return result;
	}

	bool DrawImportSettings(ImportSettings& settings)
	{
		bool changed = false;
		std::string currentGroup;
		for (ImportSetting& field : settings.GetFields())
		{
			if (!field.group.empty() && field.group != currentGroup)
			{
				currentGroup = field.group;
				if (ImGui::GetCursorPosY() > ImGui::GetStyle().WindowPadding.y)
					ImGui::Spacing();
				ImGui::TextDisabled("%s", currentGroup.c_str());
				ImGui::Separator();
			}
			changed |= DrawField(field, settings);
		}
		return changed;
	}
}
