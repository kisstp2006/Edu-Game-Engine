#include "PropertySerializer.h"

#include <unordered_map>

namespace EGE
{
	PropertyBag CaptureProperties(
		const TypeDescriptor& type,
		const void* object,
		bool serializedOnly)
	{
		PropertyBag result;
		for (const PropertyDescriptor& property : type.properties)
		{
			if (property.kind == PropertyKind::Unsupported ||
				(serializedOnly && !property.attributes.serialized))
			{
				continue;
			}

			PropertyValue value;
			if (property.Read(object, value))
				result.push_back({property.name, property.kind, value});
		}
		return result;
	}

	void ApplyProperties(
		const TypeDescriptor& type,
		void* object,
		const PropertyBag& properties,
		bool serializedOnly)
	{
		std::unordered_map<std::string, const PropertyState*> states;
		for (const PropertyState& state : properties)
			states[state.name] = &state;

		for (const PropertyDescriptor& property : type.properties)
		{
			if (property.kind == PropertyKind::Unsupported ||
				(serializedOnly && !property.attributes.serialized))
			{
				continue;
			}
			const auto iterator = states.find(property.name);
			if (iterator != states.end())
				property.Write(object, iterator->second->value);
		}
	}

}
