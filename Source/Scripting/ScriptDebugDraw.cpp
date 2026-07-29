#include "ScriptDebugDraw.h"

#include "../Application.h"
#include "../ModuleDebugDraw.h"
#include "ScriptMath.h"

#include <algorithm>

namespace EGE
{
	namespace
	{
		ModuleDebugDraw* ResolveDebugDraw()
		{
			return App ? App->debug_draw : nullptr;
		}

		float3 ToVector(const ScriptVector3& value)
		{
			return {value.x, value.y, value.z};
		}

		float3 ToColor(const ScriptColor& value)
		{
			return {
				std::clamp(value.r, 0.0f, 1.0f),
				std::clamp(value.g, 0.0f, 1.0f),
				std::clamp(value.b, 0.0f, 1.0f)};
		}

		constexpr DebugDrawChannel ScriptChannel =
			DebugDrawChannel::Script;

		bool GetEnabled()
		{
			ModuleDebugDraw* debugDraw = ResolveDebugDraw();
			return debugDraw &&
				debugDraw->IsChannelEnabled(ScriptChannel);
		}

		void SetEnabled(bool enabled)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
				debugDraw->SetChannelEnabled(ScriptChannel, enabled);
		}

		void Clear()
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
				debugDraw->Clear();
		}

		void DrawPoint(
			const ScriptVector3& position,
			const ScriptColor& color,
			float size,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawPoint(
					ToVector(position),
					ToColor(color),
					size,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawPointDefault(const ScriptVector3& position)
		{
			DrawPoint(
				position,
				{1.0f, 1.0f, 1.0f, 1.0f},
				3.0f,
				0.0f,
				true);
		}

		void DrawLine(
			const ScriptVector3& from,
			const ScriptVector3& to,
			const ScriptColor& color,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawLine(
					ToVector(from),
					ToVector(to),
					ToColor(color),
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawLineDefault(
			const ScriptVector3& from,
			const ScriptVector3& to)
		{
			DrawLine(
				from,
				to,
				{1.0f, 1.0f, 1.0f, 1.0f},
				0.0f,
				true);
		}

		void DrawRay(
			const ScriptVector3& origin,
			const ScriptVector3& direction,
			const ScriptColor& color,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawRay(
					ToVector(origin),
					ToVector(direction),
					ToColor(color),
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawArrow(
			const ScriptVector3& from,
			const ScriptVector3& to,
			const ScriptColor& color,
			float headSize,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawArrow(
					ToVector(from),
					ToVector(to),
					ToColor(color),
					headSize,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawCross(
			const ScriptVector3& center,
			const ScriptColor& color,
			float size,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawCross(
					ToVector(center),
					ToColor(color),
					size,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawCircle(
			const ScriptVector3& center,
			const ScriptVector3& normal,
			const ScriptColor& color,
			float radius,
			int segments,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawCircle(
					ToVector(center),
					ToVector(normal),
					ToColor(color),
					radius,
					segments,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawSphere(
			const ScriptVector3& center,
			const ScriptColor& color,
			float radius,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawSphere(
					ToVector(center),
					ToColor(color),
					radius,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawCapsule(
			const ScriptVector3& center,
			const ScriptVector3& direction,
			const ScriptColor& color,
			float radius,
			float height,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawCapsule(
					ToVector(center),
					ToVector(direction),
					ToColor(color),
					radius,
					height,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawCone(
			const ScriptVector3& apex,
			const ScriptVector3& direction,
			const ScriptColor& color,
			float baseRadius,
			float apexRadius,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawCone(
					ToVector(apex),
					ToVector(direction),
					ToColor(color),
					baseRadius,
					apexRadius,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawBox(
			const ScriptVector3& center,
			const ScriptVector3& size,
			const ScriptColor& color,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawBox(
					ToVector(center),
					ToVector(size),
					ToColor(color),
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawBounds(
			const ScriptVector3& minimum,
			const ScriptVector3& maximum,
			const ScriptColor& color,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawBounds(
					ToVector(minimum),
					ToVector(maximum),
					ToColor(color),
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawPlane(
			const ScriptVector3& center,
			const ScriptVector3& normal,
			const ScriptColor& color,
			float size,
			float normalSize,
			float duration,
			bool depthTest)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawPlane(
					ToVector(center),
					ToVector(normal),
					ToColor(color),
					size,
					normalSize,
					duration,
					depthTest,
					ScriptChannel);
			}
		}

		void DrawAxes(
			const ScriptVector3& origin,
			float size,
			float duration,
			bool depthTest)
		{
			const ScriptVector3 xEnd{
				origin.x + size, origin.y, origin.z};
			const ScriptVector3 yEnd{
				origin.x, origin.y + size, origin.z};
			const ScriptVector3 zEnd{
				origin.x, origin.y, origin.z + size};
			DrawArrow(
				origin, xEnd, {1.0f, 0.0f, 0.0f, 1.0f},
				size * 0.1f, duration, depthTest);
			DrawArrow(
				origin, yEnd, {0.0f, 1.0f, 0.0f, 1.0f},
				size * 0.1f, duration, depthTest);
			DrawArrow(
				origin, zEnd, {0.0f, 0.4f, 1.0f, 1.0f},
				size * 0.1f, duration, depthTest);
		}

		void DrawScreenText(
			const std::string& text,
			const ScriptVector3& position,
			const ScriptColor& color,
			float scale,
			float duration)
		{
			if (ModuleDebugDraw* debugDraw = ResolveDebugDraw())
			{
				debugDraw->DrawScreenText(
					text,
					float2(position.x, position.y),
					ToColor(color),
					scale,
					duration,
					ScriptChannel);
			}
		}

		bool Register(
			asIScriptEngine& engine,
			const char* declaration,
			const asSFuncPtr& function)
		{
			return engine.RegisterGlobalFunction(
				declaration,
				function,
				asCALL_CDECL) >= 0;
		}
	}

	bool RegisterDebugDrawApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		error.clear();
		engine.SetDefaultNamespace("Debug");
		const bool registered =
			Register(
				engine,
				"bool get_enabled() property",
				asFUNCTION(GetEnabled)) &&
			Register(
				engine,
				"void set_enabled(bool) property",
				asFUNCTION(SetEnabled)) &&
			Register(engine, "void Clear()", asFUNCTION(Clear)) &&
			Register(
				engine,
				"void DrawPoint(const Vector3 &in position)",
				asFUNCTION(DrawPointDefault)) &&
			Register(
				engine,
				"void DrawPoint(const Vector3 &in position, "
				"const Color &in color, float size = 3, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawPoint)) &&
			Register(
				engine,
				"void DrawLine(const Vector3 &in from, "
				"const Vector3 &in to)",
				asFUNCTION(DrawLineDefault)) &&
			Register(
				engine,
				"void DrawLine(const Vector3 &in from, "
				"const Vector3 &in to, const Color &in color, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawLine)) &&
			Register(
				engine,
				"void DrawRay(const Vector3 &in origin, "
				"const Vector3 &in direction, const Color &in color, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawRay)) &&
			Register(
				engine,
				"void DrawArrow(const Vector3 &in from, "
				"const Vector3 &in to, const Color &in color, "
				"float headSize = 0.1, float duration = 0, "
				"bool depthTest = true)",
				asFUNCTION(DrawArrow)) &&
			Register(
				engine,
				"void DrawCross(const Vector3 &in center, "
				"const Color &in color, float size = 1, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawCross)) &&
			Register(
				engine,
				"void DrawCircle(const Vector3 &in center, "
				"const Vector3 &in normal, const Color &in color, "
				"float radius, int segments = 32, float duration = 0, "
				"bool depthTest = true)",
				asFUNCTION(DrawCircle)) &&
			Register(
				engine,
				"void DrawSphere(const Vector3 &in center, "
				"const Color &in color, float radius, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawSphere)) &&
			Register(
				engine,
				"void DrawCapsule(const Vector3 &in center, "
				"const Vector3 &in direction, const Color &in color, "
				"float radius, float height, float duration = 0, "
				"bool depthTest = true)",
				asFUNCTION(DrawCapsule)) &&
			Register(
				engine,
				"void DrawCone(const Vector3 &in apex, "
				"const Vector3 &in direction, const Color &in color, "
				"float baseRadius, float apexRadius = 0, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawCone)) &&
			Register(
				engine,
				"void DrawBox(const Vector3 &in center, "
				"const Vector3 &in size, const Color &in color, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawBox)) &&
			Register(
				engine,
				"void DrawBounds(const Vector3 &in minimum, "
				"const Vector3 &in maximum, const Color &in color, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawBounds)) &&
			Register(
				engine,
				"void DrawPlane(const Vector3 &in center, "
				"const Vector3 &in normal, const Color &in color, "
				"float size, float normalSize = 0, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawPlane)) &&
			Register(
				engine,
				"void DrawAxes(const Vector3 &in origin, float size = 1, "
				"float duration = 0, bool depthTest = true)",
				asFUNCTION(DrawAxes)) &&
			Register(
				engine,
				"void DrawScreenText(const string &in text, "
				"const Vector3 &in position, const Color &in color, "
				"float scale = 1, float duration = 0)",
				asFUNCTION(DrawScreenText));
		engine.SetDefaultNamespace("");

		if (!registered)
		{
			error = "Could not register the Debug Draw API.";
			return false;
		}
		return true;
	}
}
