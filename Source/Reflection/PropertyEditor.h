#ifndef EGE_PROPERTY_EDITOR_H
#define EGE_PROPERTY_EDITOR_H

#include "TypeRegistry.h"

namespace EGE
{
	bool DrawReflectedProperties(
		const TypeDescriptor& type,
		void* object);
}

#endif
