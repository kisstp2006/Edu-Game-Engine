#ifndef EGE_PROPERTY_SERIALIZER_H
#define EGE_PROPERTY_SERIALIZER_H

#include "TypeRegistry.h"

class Config;

namespace EGE
{
	[[nodiscard]] PropertyBag CaptureProperties(
		const TypeDescriptor& type,
		const void* object,
		bool serializedOnly);
	void ApplyProperties(
		const TypeDescriptor& type,
		void* object,
		const PropertyBag& properties,
		bool serializedOnly);
	void SavePropertyBag(
		Config& config,
		const char* arrayName,
		const PropertyBag& properties);
	[[nodiscard]] PropertyBag LoadPropertyBag(
		const Config& config,
		const char* arrayName);
}

#endif
