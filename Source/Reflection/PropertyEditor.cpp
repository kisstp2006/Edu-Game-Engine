#include "PropertyEditor.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cstdint>
#include <string>

namespace EGE
{
	namespace
	{
		std::string FormatValue(const PropertyValue& value)
		{
			if (const auto* current = std::get_if<bool>(&value))
				return *current ? "True" : "False";
			if (const auto* current = std::get_if<std::int64_t>(&value))
				return std::to_string(*current);
			if (const auto* current = std::get_if<std::uint64_t>(&value))
				return std::to_string(*current);
			if (const auto* current = std::get_if<double>(&value))
				return std::to_string(*current);
			if (const auto* current = std::get_if<std::string>(&value))
				return *current;
			return {};
		}

		bool DrawSigned(
			const PropertyDescriptor& property,
			std::int64_t& value)
		{
			if (!property.attributes.range)
				return ImGui::InputScalar(
					"##Value", ImGuiDataType_S64, &value);

			std::int64_t minimum = static_cast<std::int64_t>(
				property.attributes.range->minimum);
			std::int64_t maximum = static_cast<std::int64_t>(
				property.attributes.range->maximum);
			return ImGui::SliderScalar(
				"##Value",
				ImGuiDataType_S64,
				&value,
				&minimum,
				&maximum);
		}

		bool DrawUnsigned(
			const PropertyDescriptor& property,
			std::uint64_t& value)
		{
			if (!property.attributes.range)
				return ImGui::InputScalar(
					"##Value", ImGuiDataType_U64, &value);

			const double lower = std::max(
				0.0, property.attributes.range->minimum);
			const double upper = std::max(
				lower, property.attributes.range->maximum);
			std::uint64_t minimum = static_cast<std::uint64_t>(lower);
			std::uint64_t maximum = static_cast<std::uint64_t>(upper);
			return ImGui::SliderScalar(
				"##Value",
				ImGuiDataType_U64,
				&value,
				&minimum,
				&maximum);
		}

		bool DrawFloating(
			const PropertyDescriptor& property,
			double& value)
		{
			if (!property.attributes.range)
				return ImGui::DragScalar(
					"##Value",
					ImGuiDataType_Double,
					&value,
					0.1f);

			double minimum = property.attributes.range->minimum;
			double maximum = property.attributes.range->maximum;
			return ImGui::SliderScalar(
				"##Value",
				ImGuiDataType_Double,
				&value,
				&minimum,
				&maximum);
		}

		bool DrawProperty(
			const PropertyDescriptor& property,
			void* object)
		{
			PropertyValue value;
			if (!property.Read(object, value))
				return false;
			PropertyValue normalized;
			if (!CoercePropertyValue(
					property.kind, value, normalized))
			{
				return false;
			}
			value = std::move(normalized);

			ImGui::PushID(property.name.c_str());
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(property.displayName.c_str());
			ImGui::SameLine(125.0f);
			ImGui::SetNextItemWidth(-1.0f);

			if (property.attributes.readOnly)
			{
				ImGui::TextDisabled("%s", FormatValue(value).c_str());
				ImGui::PopID();
				return false;
			}

			bool changed = false;
			switch (property.kind)
			{
				case PropertyKind::Boolean:
				{
					bool current = std::get<bool>(value);
					changed = ImGui::Checkbox("##Value", &current);
					value = current;
					break;
				}
				case PropertyKind::Int8:
				case PropertyKind::Int16:
				case PropertyKind::Int32:
				case PropertyKind::Int64:
				case PropertyKind::Enumeration:
				{
					std::int64_t current = std::get<std::int64_t>(value);
					changed = DrawSigned(property, current);
					value = current;
					break;
				}
				case PropertyKind::UInt8:
				case PropertyKind::UInt16:
				case PropertyKind::UInt32:
				case PropertyKind::UInt64:
				{
					std::uint64_t current = std::get<std::uint64_t>(value);
					changed = DrawUnsigned(property, current);
					value = current;
					break;
				}
				case PropertyKind::Float:
				case PropertyKind::Double:
				{
					double current = std::get<double>(value);
					changed = DrawFloating(property, current);
					value = current;
					break;
				}
				case PropertyKind::String:
				{
					std::string current = std::get<std::string>(value);
					changed = ImGui::InputText("##Value", &current);
					value = std::move(current);
					break;
				}
				default:
					break;
			}

			ImGui::PopID();
			return changed && property.Write(object, std::move(value));
		}
	}

	bool DrawReflectedProperties(
		const TypeDescriptor& type,
		void* object)
	{
		if (!object)
			return false;

		bool changed = false;
		std::string currentHeader;
		for (const PropertyDescriptor& property : type.properties)
		{
			if (!property.attributes.visible ||
				property.kind == PropertyKind::Unsupported)
			{
				continue;
			}

			if (!property.attributes.header.empty() &&
				property.attributes.header != currentHeader)
			{
				currentHeader = property.attributes.header;
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::TextDisabled("%s", currentHeader.c_str());
			}
			changed |= DrawProperty(property, object);
		}
		return changed;
	}
}
