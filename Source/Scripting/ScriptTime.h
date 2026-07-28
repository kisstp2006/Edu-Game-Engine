#pragma once

#include "ScriptApiRegistry.h"

namespace EGE
{
	class TimeService;

	using TimeServiceProvider = TimeService* (*)();

	void SetTimeServiceProvider(TimeServiceProvider provider);

	[[nodiscard]] bool RegisterTimeApi(
		asIScriptEngine& engine,
		std::string& error);
}
