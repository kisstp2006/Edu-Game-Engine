#pragma once

#include "ScriptApiRegistry.h"

namespace EGE
{
	[[nodiscard]] bool RegisterEngineBindings(
		asIScriptEngine& engine,
		std::string& error);
}
