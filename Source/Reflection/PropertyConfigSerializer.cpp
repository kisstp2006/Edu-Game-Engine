#include "PropertySerializer.h"

#include "../Config.h"

#include <charconv>

namespace EGE
{
	void SavePropertyBag(
		Config& config,
		const char* arrayName,
		const PropertyBag& properties)
	{
		config.AddArray(arrayName);
		for (const PropertyState& property : properties)
		{
			Config entry;
			entry.AddString("Name", property.name.c_str());
			entry.AddString("Type", PropertyKindName(property.kind).c_str());

			if (const auto* value = std::get_if<bool>(&property.value))
				entry.AddBool("Value", *value);
			else if (const auto* value = std::get_if<std::int64_t>(&property.value))
				entry.AddString("Value", std::to_string(*value).c_str());
			else if (const auto* value = std::get_if<std::uint64_t>(&property.value))
				entry.AddString("Value", std::to_string(*value).c_str());
			else if (const auto* value = std::get_if<double>(&property.value))
				entry.AddDouble("Value", *value);
			else if (const auto* value = std::get_if<std::string>(&property.value))
				entry.AddString("Value", value->c_str());
			else if (const auto* value =
				std::get_if<GameObjectReferenceValue>(&property.value))
			{
				entry.AddString(
					"ObjectUID",
					std::to_string(value->objectId).c_str());
			}
			else if (const auto* value =
				std::get_if<ComponentReferenceValue>(&property.value))
			{
				entry.AddString(
					"ObjectUID",
					std::to_string(value->objectId).c_str());
				entry.AddString(
					"ComponentUID",
					std::to_string(value->componentId).c_str());
			}
			else if (const auto* value =
				std::get_if<ResourceReferenceValue>(&property.value))
			{
				entry.AddString(
					"ResourceUID",
					std::to_string(value->resourceId).c_str());
				entry.AddInt("ResourceType", value->resourceType);
			}
			else if (const auto* value =
				std::get_if<Vector2Value>(&property.value))
			{
				entry.AddFloat("X", value->x);
				entry.AddFloat("Y", value->y);
			}
			else if (const auto* value =
				std::get_if<Vector3Value>(&property.value))
			{
				entry.AddFloat("X", value->x);
				entry.AddFloat("Y", value->y);
				entry.AddFloat("Z", value->z);
			}
			else if (const auto* value =
				std::get_if<QuaternionValue>(&property.value))
			{
				entry.AddFloat("X", value->x);
				entry.AddFloat("Y", value->y);
				entry.AddFloat("Z", value->z);
				entry.AddFloat("W", value->w);
			}
			else if (const auto* value =
				std::get_if<ColorValue>(&property.value))
			{
				entry.AddFloat("R", value->r);
				entry.AddFloat("G", value->g);
				entry.AddFloat("B", value->b);
				entry.AddFloat("A", value->a);
			}
			else
				continue;

			config.AddArrayEntry(entry);
		}
	}

	PropertyBag LoadPropertyBag(
		const Config& config,
		const char* arrayName)
	{
		PropertyBag result;
		const int count = config.GetArrayCount(arrayName);
		result.reserve(count);
		for (int index = 0; index < count; ++index)
		{
			Config entry = config.GetArray(arrayName, index);
			const char* name = entry.GetString("Name", "");
			const char* typeName = entry.GetString("Type", "Unsupported");
			PropertyKind kind;
			if (!name || !*name || !typeName || !ParsePropertyKind(typeName, kind))
				continue;

			PropertyValue value;
			switch (kind)
			{
				case PropertyKind::Boolean:
					value = entry.GetBool("Value", false);
					break;
				case PropertyKind::Int8:
				case PropertyKind::Int16:
				case PropertyKind::Int32:
				case PropertyKind::Int64:
				case PropertyKind::Enumeration:
				{
					std::int64_t parsed = 0;
					const char* text = entry.GetString("Value", nullptr);
					if (text)
					{
						const char* end = text + std::char_traits<char>::length(text);
						std::from_chars(text, end, parsed);
					}
					else
					{
						parsed = static_cast<std::int64_t>(
							entry.GetDouble("Value", 0.0));
					}
					value = parsed;
					break;
				}
				case PropertyKind::UInt8:
				case PropertyKind::UInt16:
				case PropertyKind::UInt32:
				case PropertyKind::UInt64:
				{
					std::uint64_t parsed = 0;
					const char* text = entry.GetString("Value", nullptr);
					if (text)
					{
						const char* end = text + std::char_traits<char>::length(text);
						std::from_chars(text, end, parsed);
					}
					else
					{
						parsed = static_cast<std::uint64_t>(
							entry.GetDouble("Value", 0.0));
					}
					value = parsed;
					break;
				}
				case PropertyKind::Float:
				case PropertyKind::Double:
					value = entry.GetDouble("Value", 0.0);
					break;
				case PropertyKind::String:
					value = std::string(entry.GetString("Value", ""));
					break;
				case PropertyKind::GameObjectReference:
					{
						std::uint64_t objectId = 0;
						const char* text =
							entry.GetString("ObjectUID", "0");
						const char* end =
							text + std::char_traits<char>::length(text);
						std::from_chars(text, end, objectId);
						value = GameObjectReferenceValue{objectId};
						break;
					}
				case PropertyKind::ComponentReference:
					{
						std::uint64_t objectId = 0;
						std::uint64_t componentId = 0;
						const char* objectText =
							entry.GetString("ObjectUID", "0");
						const char* componentText =
							entry.GetString("ComponentUID", "0");
						std::from_chars(
							objectText,
							objectText + std::char_traits<char>::length(
								objectText),
							objectId);
						std::from_chars(
							componentText,
							componentText + std::char_traits<char>::length(
								componentText),
							componentId);
						value = ComponentReferenceValue{
							objectId, componentId};
						break;
					}
				case PropertyKind::ResourceReference:
					{
						std::uint64_t resourceId = 0;
						const char* text =
							entry.GetString("ResourceUID", "0");
						std::from_chars(
							text,
							text + std::char_traits<char>::length(text),
							resourceId);
						value = ResourceReferenceValue{
							resourceId,
							entry.GetInt("ResourceType", 0)};
						break;
					}
				case PropertyKind::Vector3:
					value = Vector3Value{
						entry.GetFloat("X", 0.0f),
						entry.GetFloat("Y", 0.0f),
						entry.GetFloat("Z", 0.0f)};
					break;
				case PropertyKind::Vector2:
					value = Vector2Value{
						entry.GetFloat("X", 0.0f),
						entry.GetFloat("Y", 0.0f)};
					break;
				case PropertyKind::Quaternion:
					value = QuaternionValue{
						entry.GetFloat("X", 0.0f),
						entry.GetFloat("Y", 0.0f),
						entry.GetFloat("Z", 0.0f),
						entry.GetFloat("W", 1.0f)};
					break;
				case PropertyKind::Color:
					value = ColorValue{
						entry.GetFloat("R", 0.0f),
						entry.GetFloat("G", 0.0f),
						entry.GetFloat("B", 0.0f),
						entry.GetFloat("A", 1.0f)};
					break;
				default:
					continue;
			}
			result.push_back({name, kind, std::move(value)});
		}
		return result;
	}
}
