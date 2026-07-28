#pragma once

#include "ScriptApiRegistry.h"

namespace EGE
{
	[[nodiscard]] bool RegisterCoreHelpersApi(
		asIScriptEngine& engine,
		std::string& error);
}
