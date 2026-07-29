#pragma once

#include <angelscript.h>

#include <string>

namespace EGE
{
	struct ScriptVector2
	{
		float x = 0.0f;
		float y = 0.0f;
		float abiPadding = 0.0f;
	};

	struct ScriptVector3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct ScriptColor
	{
		float r = 0.0f;
		float g = 0.0f;
		float b = 0.0f;
		float a = 1.0f;
	};

	struct ScriptQuaternion
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;
	};

	[[nodiscard]] bool RegisterMathApi(
		asIScriptEngine& engine,
		std::string& error);

	[[nodiscard]] bool RegisterExtendedMathApi(
		asIScriptEngine& engine,
		std::string& error);
}
