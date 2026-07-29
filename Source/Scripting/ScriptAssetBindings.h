#pragma once

#include "../Resource.h"

#include <cstdint>
#include <string>

class asIScriptEngine;

namespace EGE
{
	class ScriptResourceReference;

	[[nodiscard]] ScriptResourceReference*
	GetScriptResourceReference(
		std::uint64_t id,
		Resource::Type type);

	[[nodiscard]] ScriptResourceReference*
	FindScriptResourceReference(
		const std::string& path,
		Resource::Type type);

	[[nodiscard]] UID ResolveScriptResourceId(
		const ScriptResourceReference* reference,
		Resource::Type expectedType);

	[[nodiscard]] bool RegisterScriptAssetApi(
		asIScriptEngine& engine,
		std::string& error);
}
