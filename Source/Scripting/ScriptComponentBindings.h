#pragma once

#include <string>

class asIScriptEngine;

namespace EGE
{
	[[nodiscard]] bool RegisterTypedComponentApi(
		asIScriptEngine& engine,
		std::string& error);
}
