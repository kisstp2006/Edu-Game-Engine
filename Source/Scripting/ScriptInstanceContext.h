#pragma once

#include <filesystem>
#include <optional>
#include <string>

class asIScriptContext;
class asIScriptEngine;
class asIScriptFunction;

namespace EGE
{
	struct ScriptExecutionError
	{
		std::filesystem::path file;
		int line = 0;
		int column = 0;
		std::string message;
		std::string function;
	};

	class ScriptInstanceContext final
	{
	public:
		ScriptInstanceContext() = default;
		~ScriptInstanceContext();

		ScriptInstanceContext(const ScriptInstanceContext&) = delete;
		ScriptInstanceContext& operator=(const ScriptInstanceContext&) = delete;

		[[nodiscard]] bool Initialize(
			asIScriptEngine& engine,
			std::string& error);
		void Reset();
		[[nodiscard]] bool Execute(
			asIScriptFunction& function,
			void* object,
			std::optional<float> deltaTime,
			ScriptExecutionError& error);
		[[nodiscard]] bool ExecuteWithObject(
			asIScriptFunction& function,
			void* object,
			void* argument,
			ScriptExecutionError& error);
		[[nodiscard]] bool ExecuteWithObjects(
			asIScriptFunction& function,
			void* object,
			void* firstArgument,
			void* secondArgument,
			ScriptExecutionError& error);

	private:
		asIScriptContext* context_ = nullptr;
	};
}
