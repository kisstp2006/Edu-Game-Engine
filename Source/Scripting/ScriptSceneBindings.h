#pragma once

#include <string>

class asIScriptEngine;

namespace EGE
{
	[[nodiscard]] bool RegisterSceneApi(
		asIScriptEngine& engine,
		std::string& error);
}
