#pragma once

#include <angelscript.h>

#include <string>

namespace EGE
{
	[[nodiscard]] bool RegisterDebugDrawApi(
		asIScriptEngine& engine,
		std::string& error);
}
