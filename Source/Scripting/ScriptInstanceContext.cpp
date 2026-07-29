#include "ScriptInstanceContext.h"

#include <angelscript.h>

namespace EGE
{
	ScriptInstanceContext::~ScriptInstanceContext()
	{
		Reset();
	}

	bool ScriptInstanceContext::Initialize(
		asIScriptEngine& engine,
		std::string& error)
	{
		Reset();
		context_ = engine.CreateContext();
		if (context_)
			return true;

		error = "Could not create an AngelScript execution context.";
		return false;
	}

	void ScriptInstanceContext::Reset()
	{
		if (!context_)
			return;
		context_->Release();
		context_ = nullptr;
	}

	bool ScriptInstanceContext::Execute(
		asIScriptFunction& function,
		void* object,
		std::optional<float> deltaTime,
		ScriptExecutionError& error)
	{
		error = {};
		if (!context_)
		{
			error.message = "The AngelScript execution context is unavailable.";
			return false;
		}

		const bool restorePreviousState =
			context_->GetState() == asEXECUTION_ACTIVE;
		if (restorePreviousState && context_->PushState() < 0)
		{
			error.message = "Could not preserve the active AngelScript call.";
			return false;
		}

		auto restore = [this, restorePreviousState]()
		{
			context_->Unprepare();
			if (restorePreviousState)
				context_->PopState();
		};

		if (context_->Prepare(&function) < 0)
		{
			error.message = "Could not prepare " +
				std::string(function.GetDeclaration());
			if (restorePreviousState)
				context_->PopState();
			return false;
		}
		if (object && context_->SetObject(object) < 0)
		{
			error.message = "Could not bind the script instance to " +
				std::string(function.GetDeclaration());
			restore();
			return false;
		}
		if (deltaTime && context_->SetArgFloat(0, *deltaTime) < 0)
		{
			error.message = "Could not pass delta time to " +
				std::string(function.GetDeclaration());
			restore();
			return false;
		}

		const int result = context_->Execute();
		if (result == asEXECUTION_FINISHED)
		{
			restore();
			return true;
		}

		error.function = function.GetDeclaration();
		if (result == asEXECUTION_EXCEPTION)
		{
			const char* section = nullptr;
			error.line = context_->GetLineNumber(
				0, &error.column, &section);
			if (section)
				error.file = section;
			const char* exception = context_->GetExceptionString();
			error.message = exception
				? exception
				: "AngelScript execution raised an exception.";
		}
		else
		{
			error.message = "AngelScript execution ended with code " +
				std::to_string(result) + ".";
		}
		restore();
		return false;
	}

	bool ScriptInstanceContext::ExecuteWithObjects(
		asIScriptFunction& function,
		void* object,
		void* firstArgument,
		void* secondArgument,
		ScriptExecutionError& error)
	{
		error = {};
		if (!context_ || context_->Prepare(&function) < 0 ||
			(object && context_->SetObject(object) < 0) ||
			context_->SetArgObject(0, firstArgument) < 0 ||
			context_->SetArgObject(1, secondArgument) < 0)
		{
			error.message = "Could not bind the EGEBehaviour owner.";
			if (context_)
				context_->Unprepare();
			return false;
		}

		const int result = context_->Execute();
		context_->Unprepare();
		if (result == asEXECUTION_FINISHED)
			return true;
		error.function = function.GetDeclaration();
		error.message = "Could not execute the EGEBehaviour owner binding.";
		return false;
	}

	bool ScriptInstanceContext::ExecuteWithObject(
		asIScriptFunction& function,
		void* object,
		void* argument,
		ScriptExecutionError& error)
	{
		error = {};
		if (!context_)
		{
			error.message = "The AngelScript execution context is unavailable.";
			return false;
		}

		const bool restorePreviousState =
			context_->GetState() == asEXECUTION_ACTIVE;
		if (restorePreviousState && context_->PushState() < 0)
		{
			error.message = "Could not preserve the active AngelScript call.";
			return false;
		}

		auto restore = [this, restorePreviousState]()
		{
			context_->Unprepare();
			if (restorePreviousState)
				context_->PopState();
		};

		if (context_->Prepare(&function) < 0)
		{
			error.message = "Could not prepare " +
				std::string(function.GetDeclaration());
			if (restorePreviousState)
				context_->PopState();
			return false;
		}
		if (object && context_->SetObject(object) < 0)
		{
			error.message = "Could not bind the script instance to " +
				std::string(function.GetDeclaration());
			restore();
			return false;
		}
		if (context_->SetArgObject(0, argument) < 0)
		{
			error.message = "Could not pass the object argument to " +
				std::string(function.GetDeclaration());
			restore();
			return false;
		}

		const int result = context_->Execute();
		if (result == asEXECUTION_FINISHED)
		{
			restore();
			return true;
		}

		error.function = function.GetDeclaration();
		if (result == asEXECUTION_EXCEPTION)
		{
			const char* section = nullptr;
			error.line = context_->GetLineNumber(
				0, &error.column, &section);
			if (section)
				error.file = section;
			const char* exception = context_->GetExceptionString();
			error.message = exception
				? exception
				: "AngelScript execution raised an exception.";
		}
		else
		{
			error.message = "AngelScript execution ended with code " +
				std::to_string(result) + ".";
		}
		restore();
		return false;
	}
}
