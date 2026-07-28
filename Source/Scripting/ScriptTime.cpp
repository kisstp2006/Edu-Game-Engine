#include "ScriptTime.h"

#include "../EngineTime.h"

#include <angelscript.h>

namespace EGE
{
	namespace
	{
		TimeServiceProvider timeServiceProvider = nullptr;

		TimeService* ResolveTimeService()
		{
			return timeServiceProvider ? timeServiceProvider() : nullptr;
		}

		float GetDeltaTime()
		{
			const TimeService* time = ResolveTimeService();
			return time ? time->GetDeltaTime() : 0.0f;
		}

		float GetUnscaledDeltaTime()
		{
			const TimeService* time = ResolveTimeService();
			return time ? time->GetUnscaledDeltaTime() : 0.0f;
		}

		float GetFixedDeltaTime()
		{
			const TimeService* time = ResolveTimeService();
			return time
				? time->GetFixedDeltaTime()
				: TimeService::DefaultFixedDeltaTime;
		}

		double GetTime()
		{
			const TimeService* time = ResolveTimeService();
			return time ? time->GetTime() : 0.0;
		}

		double GetUnscaledTime()
		{
			const TimeService* time = ResolveTimeService();
			return time ? time->GetUnscaledTime() : 0.0;
		}

		float GetTimeScale()
		{
			const TimeService* time = ResolveTimeService();
			return time ? time->GetTimeScale() : 1.0f;
		}

		void SetTimeScale(float value)
		{
			if (TimeService* time = ResolveTimeService())
				time->SetTimeScale(value);
		}

		asQWORD GetFrameCount()
		{
			const TimeService* time = ResolveTimeService();
			return time
				? static_cast<asQWORD>(time->GetFrameCount())
				: 0;
		}

		bool GetIsPlaying()
		{
			const TimeService* time = ResolveTimeService();
			return time && time->IsPlaying();
		}

		bool GetIsPaused()
		{
			const TimeService* time = ResolveTimeService();
			return time && time->IsPaused();
		}
	}

	void SetTimeServiceProvider(TimeServiceProvider provider)
	{
		timeServiceProvider = provider;
	}

	bool RegisterTimeApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		error.clear();
		engine.SetDefaultNamespace("Time");
		const bool registered =
			engine.RegisterGlobalFunction(
				"float get_deltaTime() property",
				asFUNCTION(GetDeltaTime), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"float get_unscaledDeltaTime() property",
				asFUNCTION(GetUnscaledDeltaTime), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"float get_fixedDeltaTime() property",
				asFUNCTION(GetFixedDeltaTime), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"double get_time() property",
				asFUNCTION(GetTime), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"double get_unscaledTime() property",
				asFUNCTION(GetUnscaledTime), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"float get_timeScale() property",
				asFUNCTION(GetTimeScale), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"void set_timeScale(float value) property",
				asFUNCTION(SetTimeScale), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"uint64 get_frameCount() property",
				asFUNCTION(GetFrameCount), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"bool get_isPlaying() property",
				asFUNCTION(GetIsPlaying), asCALL_CDECL) >= 0 &&
			engine.RegisterGlobalFunction(
				"bool get_isPaused() property",
				asFUNCTION(GetIsPaused), asCALL_CDECL) >= 0;
		engine.SetDefaultNamespace("");

		if (registered)
			return true;

		error = "Could not register the Time API.";
		return false;
	}
}
