#include "PropertyEditor.h"

#include "../Application.h"
#include "../Component.h"
#include "../GameObject.h"
#include "../ModuleLevelManager.h"

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
			if (const auto* current =
				std::get_if<GameObjectReferenceValue>(&value))
			{
				const GameObject* gameObject =
					App && App->level
						? App->level->Find(
							static_cast<uint>(current->objectId))
						: nullptr;
				return gameObject
					? gameObject->name
					: current->objectId == 0
						? "None"
						: "Missing (" +
							std::to_string(current->objectId) + ")";
			}
			if (const auto* current =
				std::get_if<ComponentReferenceValue>(&value))
			{
				const GameObject* gameObject =
					App && App->level
						? App->level->Find(
							static_cast<uint>(current->objectId))
						: nullptr;
				if (gameObject)
				{
					for (const Component* component :
						gameObject->components)
					{
						if (component &&
							component->GetUID() ==
								current->componentId)
						{
							return gameObject->name + " / " +
								component->GetTypeStr();
						}
					}
				}
				return current->componentId == 0
					? "None"
					: "Missing (" +
						std::to_string(current->componentId) + ")";
			}
			if (const auto* current = std::get_if<Vector3Value>(&value))
			{
				return "(" + std::to_string(current->x) + ", " +
					std::to_string(current->y) + ", " +
					std::to_string(current->z) + ")";
			}
			if (const auto* current = std::get_if<ColorValue>(&value))
			{
				return "(" + std::to_string(current->r) + ", " +
					std::to_string(current->g) + ", " +
					std::to_string(current->b) + ", " +
					std::to_string(current->a) + ")";
			}
			return {};
		}

		bool DrawGameObjectEntry(
			const GameObject& gameObject,
			int depth,
			GameObjectReferenceValue& value)
		{
			bool changed = false;
			ImGui::PushID(gameObject.GetUID());
			const std::string label(
				static_cast<std::size_t>(depth) * 2, ' ');
			const bool selected =
				value.objectId == gameObject.GetUID();
			if (ImGui::Selectable(
					(label + gameObject.name).c_str(), selected))
			{
				value.objectId = gameObject.GetUID();
				changed = true;
			}
			ImGui::PopID();

			for (const GameObject* child : gameObject.childs)
			{
				if (child && !child->IsPendingDestroy())
				changed |= DrawGameObjectEntry(*child, depth + 1, value);
			}
			return changed;
		}

		bool DrawGameObjectReference(
			GameObjectReferenceValue& value)
		{
			const std::string preview =
				FormatValue(PropertyValue(value));
			bool changed = false;
			if (ImGui::BeginCombo("##Value", preview.c_str()))
			{
				if (ImGui::Selectable("None", value.objectId == 0))
				{
					value.objectId = 0;
					changed = true;
				}

				if (App && App->level && App->level->GetRoot())
				{
					for (const GameObject* gameObject :
						App->level->GetRoot()->childs)
					{
						if (gameObject &&
							!gameObject->IsPendingDestroy())
						{
							changed |= DrawGameObjectEntry(
								*gameObject, 0, value);
						}
					}
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		bool DrawComponentEntries(
			const GameObject& gameObject,
			ComponentReferenceValue& value)
		{
			bool changed = false;
			for (const Component* component : gameObject.components)
			{
				if (!component)
					continue;

				ImGui::PushID(component->GetUID());
				const std::string label =
					gameObject.name + " / " + component->GetTypeStr();
				const bool selected =
					value.objectId == gameObject.GetUID() &&
					value.componentId == component->GetUID();
				if (ImGui::Selectable(label.c_str(), selected))
				{
					value.objectId = gameObject.GetUID();
					value.componentId = component->GetUID();
					changed = true;
				}
				ImGui::PopID();
			}

			for (const GameObject* child : gameObject.childs)
			{
				if (child && !child->IsPendingDestroy())
					changed |= DrawComponentEntries(*child, value);
			}
			return changed;
		}

		bool DrawComponentReference(
			ComponentReferenceValue& value)
		{
			const std::string preview =
				FormatValue(PropertyValue(value));
			bool changed = false;
			if (ImGui::BeginCombo("##Value", preview.c_str()))
			{
				if (ImGui::Selectable(
						"None", value.componentId == 0))
				{
					value = {};
					changed = true;
				}

				if (App && App->level && App->level->GetRoot())
				{
					for (const GameObject* gameObject :
						App->level->GetRoot()->childs)
					{
						if (gameObject &&
							!gameObject->IsPendingDestroy())
						{
							changed |= DrawComponentEntries(
								*gameObject, value);
						}
					}
				}
				ImGui::EndCombo();
			}
			return changed;
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

		bool DrawEnumeration(
			const PropertyDescriptor& property,
			std::int64_t& value)
		{
			if (property.enumValues.empty())
				return DrawSigned(property, value);

			std::string preview = std::to_string(value);
			for (const PropertyEnumValue& option : property.enumValues)
			{
				if (option.value == value)
				{
					preview = option.displayName;
					break;
				}
			}

			bool changed = false;
			if (ImGui::BeginCombo("##Value", preview.c_str()))
			{
				for (const PropertyEnumValue& option :
					property.enumValues)
				{
					const bool selected = option.value == value;
					ImGui::PushID(option.name.c_str());
					if (ImGui::Selectable(
							option.displayName.c_str(), selected))
					{
						value = option.value;
						changed = true;
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			return changed;
		}

		bool DrawVector3(
			const PropertyDescriptor& property,
			Vector3Value& value)
		{
			float components[] = {value.x, value.y, value.z};
			const bool changed = property.attributes.range
				? ImGui::SliderFloat3(
					"##Value",
					components,
					static_cast<float>(
						property.attributes.range->minimum),
					static_cast<float>(
						property.attributes.range->maximum))
				: ImGui::DragFloat3(
					"##Value", components, 0.1f);
			if (changed)
				value = {components[0], components[1], components[2]};
			return changed;
		}

		bool DrawColor(ColorValue& value)
		{
			float components[] = {value.r, value.g, value.b, value.a};
			const bool changed = ImGui::ColorEdit4(
				"##Value",
				components,
				ImGuiColorEditFlags_AlphaBar |
					ImGuiColorEditFlags_Float);
			if (changed)
			{
				value = {
					components[0],
					components[1],
					components[2],
					components[3]};
			}
			return changed;
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
			if (!property.attributes.tooltip.empty() &&
				ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"%s", property.attributes.tooltip.c_str());
			}
			const float labelColumnWidth = 125.0f;
			const float labelWidth =
				ImGui::CalcTextSize(property.displayName.c_str()).x;
			if (labelWidth <=
				labelColumnWidth - ImGui::GetStyle().ItemSpacing.x)
			{
				ImGui::SameLine(labelColumnWidth);
			}
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
				{
					std::int64_t current = std::get<std::int64_t>(value);
					changed = DrawSigned(property, current);
					value = current;
					break;
				}
				case PropertyKind::Enumeration:
				{
					std::int64_t current = std::get<std::int64_t>(value);
					changed = DrawEnumeration(property, current);
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
				case PropertyKind::GameObjectReference:
					{
						auto current =
							std::get<GameObjectReferenceValue>(value);
						changed = DrawGameObjectReference(current);
						value = current;
						break;
					}
				case PropertyKind::ComponentReference:
					{
						auto current =
							std::get<ComponentReferenceValue>(value);
						changed = DrawComponentReference(current);
						value = current;
						break;
					}
				case PropertyKind::Vector3:
					{
						auto current = std::get<Vector3Value>(value);
						changed = DrawVector3(property, current);
						value = current;
						break;
					}
				case PropertyKind::Color:
					{
						auto current = std::get<ColorValue>(value);
						changed = DrawColor(current);
						value = current;
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
		bool drewProperty = false;
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
			drewProperty = true;
			changed |= DrawProperty(property, object);
		}
		if (!drewProperty)
			ImGui::TextDisabled("No serialized properties.");
		return changed;
	}
}
