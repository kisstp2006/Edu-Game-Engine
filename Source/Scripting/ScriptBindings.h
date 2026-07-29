#pragma once

#include "../EngineAPI.h"
#include "ScriptApiRegistry.h"

namespace EGE
{
	[[nodiscard]] EGE_API bool RegisterEngineBindings(
		asIScriptEngine& engine,
		std::string& error);
	[[nodiscard]] EGE_API bool RunEngineBindingsSelfTest();
}
