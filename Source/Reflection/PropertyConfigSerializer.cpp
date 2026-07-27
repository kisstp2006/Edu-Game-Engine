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
				default:
					continue;
			}
			result.push_back({name, kind, std::move(value)});
		}
		return result;
	}
}
