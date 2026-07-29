#include "ScriptMath.h"

#include "../Globals.h"
#include "../Math.h"

#include <algorithm>
#include <cmath>
#include <new>

namespace EGE
{
	namespace
	{
		constexpr float Epsilon = 0.00001f;
		const ScriptVector2 Vector2Zero{};
		const ScriptVector2 Vector2One{1.0f, 1.0f};
		const ScriptVector2 Vector2Up{0.0f, 1.0f};
		const ScriptVector2 Vector2Down{0.0f, -1.0f};
		const ScriptVector2 Vector2Left{-1.0f, 0.0f};
		const ScriptVector2 Vector2Right{1.0f, 0.0f};
		const ScriptQuaternion QuaternionIdentity{};

		float2 ToNative(const ScriptVector2& value)
		{
			return {value.x, value.y};
		}

		float3 ToNative(const ScriptVector3& value)
		{
			return {value.x, value.y, value.z};
		}

		Quat ToNative(const ScriptQuaternion& value)
		{
			return {value.x, value.y, value.z, value.w};
		}

		ScriptVector2 ToScript(const float2& value)
		{
			return {value.x, value.y};
		}

		ScriptVector3 ToScript(const float3& value)
		{
			return {value.x, value.y, value.z};
		}

		ScriptQuaternion ToScript(const Quat& value)
		{
			return {value.x, value.y, value.z, value.w};
		}

		void ConstructVector2(float x, float y, ScriptVector2* value)
		{
			new (value) ScriptVector2{x, y};
		}

		void CopyConstructVector2(
			const ScriptVector2& source,
			ScriptVector2* value)
		{
			new (value) ScriptVector2(source);
		}

		void DestructVector2(ScriptVector2* value)
		{
			value->~ScriptVector2();
		}

		ScriptVector2& AssignVector2(
			const ScriptVector2& source,
			ScriptVector2* value)
		{
			*value = source;
			return *value;
		}

		ScriptVector2 AddVector2(
			const ScriptVector2& right,
			const ScriptVector2& left)
		{
			return {left.x + right.x, left.y + right.y};
		}

		ScriptVector2 SubtractVector2(
			const ScriptVector2& right,
			const ScriptVector2& left)
		{
			return {left.x - right.x, left.y - right.y};
		}

		ScriptVector2 NegateVector2(const ScriptVector2& value)
		{
			return {-value.x, -value.y};
		}

		ScriptVector2 MultiplyVector2(float scalar, const ScriptVector2& value)
		{
			return {value.x * scalar, value.y * scalar};
		}

		ScriptVector2 DivideVector2(float scalar, const ScriptVector2& value)
		{
			return std::abs(scalar) > Epsilon
				? ScriptVector2{value.x / scalar, value.y / scalar}
				: ScriptVector2{};
		}

		ScriptVector2& AddAssignVector2(
			const ScriptVector2& right,
			ScriptVector2* left)
		{
			*left = AddVector2(right, *left);
			return *left;
		}

		ScriptVector2& SubtractAssignVector2(
			const ScriptVector2& right,
			ScriptVector2* left)
		{
			*left = SubtractVector2(right, *left);
			return *left;
		}

		ScriptVector2& MultiplyAssignVector2(
			float scalar,
			ScriptVector2* value)
		{
			*value = MultiplyVector2(scalar, *value);
			return *value;
		}

		ScriptVector2& DivideAssignVector2(
			float scalar,
			ScriptVector2* value)
		{
			*value = DivideVector2(scalar, *value);
			return *value;
		}

		bool EqualVector2(
			const ScriptVector2& right,
			const ScriptVector2& left)
		{
			return left.x == right.x && left.y == right.y;
		}

		float DotVector2(
			const ScriptVector2& left,
			const ScriptVector2& right)
		{
			return left.x * right.x + left.y * right.y;
		}

		float LengthSquaredVector2(const ScriptVector2& value)
		{
			return DotVector2(value, value);
		}

		float LengthVector2(const ScriptVector2& value)
		{
			return std::sqrt(LengthSquaredVector2(value));
		}

		ScriptVector2 NormalizeVector2(const ScriptVector2& value)
		{
			const float length = LengthVector2(value);
			return length > Epsilon
				? DivideVector2(length, value)
				: ScriptVector2{};
		}

		void NormalizeVector2InPlace(ScriptVector2* value)
		{
			*value = NormalizeVector2(*value);
		}

		bool IsFiniteVector2(const ScriptVector2& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y);
		}

		float DistanceVector2(
			const ScriptVector2& left,
			const ScriptVector2& right)
		{
			return LengthVector2(SubtractVector2(left, right));
		}

		ScriptVector2 LerpVector2(
			const ScriptVector2& start,
			const ScriptVector2& end,
			float amount)
		{
			return AddVector2(
				MultiplyVector2(
					std::clamp(amount, 0.0f, 1.0f),
					SubtractVector2(start, end)),
				start);
		}

		ScriptVector2 LerpUnclampedVector2(
			const ScriptVector2& start,
			const ScriptVector2& end,
			float amount)
		{
			return AddVector2(
				MultiplyVector2(amount, SubtractVector2(start, end)),
				start);
		}

		ScriptVector2 MoveTowardsVector2(
			const ScriptVector2& current,
			const ScriptVector2& target,
			float maxDistanceDelta)
		{
			const ScriptVector2 delta = SubtractVector2(current, target);
			const float distance = LengthVector2(delta);
			if (distance <= maxDistanceDelta || distance <= Epsilon)
				return target;
			return AddVector2(
				MultiplyVector2(maxDistanceDelta / distance, delta),
				current);
		}

		float AngleVector2(
			const ScriptVector2& left,
			const ScriptVector2& right)
		{
			const float denominator =
				LengthVector2(left) * LengthVector2(right);
			if (denominator <= Epsilon)
				return 0.0f;
			return std::acos(std::clamp(
				DotVector2(left, right) / denominator,
				-1.0f,
				1.0f)) * RADTODEG;
		}

		void ConstructQuaternion(
			float x,
			float y,
			float z,
			float w,
			ScriptQuaternion* value)
		{
			new (value) ScriptQuaternion{x, y, z, w};
		}

		void CopyConstructQuaternion(
			const ScriptQuaternion& source,
			ScriptQuaternion* value)
		{
			new (value) ScriptQuaternion(source);
		}

		void DestructQuaternion(ScriptQuaternion* value)
		{
			value->~ScriptQuaternion();
		}

		ScriptQuaternion& AssignQuaternion(
			const ScriptQuaternion& source,
			ScriptQuaternion* value)
		{
			*value = source;
			return *value;
		}

		ScriptQuaternion MultiplyQuaternion(
			const ScriptQuaternion& right,
			const ScriptQuaternion& left)
		{
			return ToScript(ToNative(left) * ToNative(right));
		}

		ScriptVector3 RotateVector(
			const ScriptVector3& vector,
			const ScriptQuaternion& rotation)
		{
			return ToScript(ToNative(rotation).Transform(ToNative(vector)));
		}

		bool EqualQuaternion(
			const ScriptQuaternion& right,
			const ScriptQuaternion& left)
		{
			return left.x == right.x && left.y == right.y &&
				left.z == right.z && left.w == right.w;
		}

		float LengthSquaredQuaternion(const ScriptQuaternion& value)
		{
			const Quat native = ToNative(value);
			return native.LengthSq();
		}

		ScriptQuaternion NormalizeQuaternion(
			const ScriptQuaternion& value)
		{
			const Quat native = ToNative(value);
			return native.LengthSq() > Epsilon * Epsilon
				? ToScript(native.Normalized())
				: QuaternionIdentity;
		}

		void NormalizeQuaternionInPlace(ScriptQuaternion* value)
		{
			*value = NormalizeQuaternion(*value);
		}

		bool IsFiniteQuaternion(const ScriptQuaternion& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y) &&
				std::isfinite(value.z) && std::isfinite(value.w);
		}

		ScriptVector3 GetQuaternionEulerAngles(
			const ScriptQuaternion& value)
		{
			return ToScript(ToNative(NormalizeQuaternion(value)).ToEulerXYZ());
		}

		ScriptQuaternion QuaternionEuler(const ScriptVector3& radians)
		{
			return ToScript(Quat::FromEulerXYZ(
				radians.x, radians.y, radians.z));
		}

		ScriptQuaternion QuaternionAngleAxis(
			float radians,
			const ScriptVector3& axis)
		{
			const float3 nativeAxis = ToNative(axis);
			return nativeAxis.LengthSq() > Epsilon * Epsilon
				? ToScript(Quat::RotateAxisAngle(
					nativeAxis.Normalized(), radians))
				: QuaternionIdentity;
		}

		ScriptQuaternion QuaternionFromToRotation(
			const ScriptVector3& from,
			const ScriptVector3& to)
		{
			const float3 nativeFrom = ToNative(from);
			const float3 nativeTo = ToNative(to);
			if (nativeFrom.LengthSq() <= Epsilon * Epsilon ||
				nativeTo.LengthSq() <= Epsilon * Epsilon)
			{
				return QuaternionIdentity;
			}
			return ToScript(Quat::RotateFromTo(
				nativeFrom.Normalized(), nativeTo.Normalized()));
		}

		ScriptQuaternion QuaternionLookRotation(
			const ScriptVector3& forward,
			const ScriptVector3& up)
		{
			const float3 nativeForward = ToNative(forward);
			const float3 nativeUp = ToNative(up);
			if (nativeForward.LengthSq() <= Epsilon * Epsilon ||
				nativeUp.LengthSq() <= Epsilon * Epsilon)
			{
				return QuaternionIdentity;
			}
			return ToScript(Quat::LookAt(
				-float3::unitZ,
				nativeForward.Normalized(),
				float3::unitY,
				nativeUp.Normalized()));
		}

		ScriptQuaternion QuaternionInverse(const ScriptQuaternion& value)
		{
			const Quat native = ToNative(value);
			return native.LengthSq() > Epsilon * Epsilon
				? ToScript(native.Inverted())
				: QuaternionIdentity;
		}

		float QuaternionDot(
			const ScriptQuaternion& left,
			const ScriptQuaternion& right)
		{
			return ToNative(left).Dot(ToNative(right));
		}

		float QuaternionAngle(
			const ScriptQuaternion& left,
			const ScriptQuaternion& right)
		{
			return ToNative(left).AngleBetween(ToNative(right));
		}

		ScriptQuaternion QuaternionLerp(
			const ScriptQuaternion& start,
			const ScriptQuaternion& end,
			float amount)
		{
			return ToScript(Quat::Lerp(
				ToNative(start),
				ToNative(end),
				std::clamp(amount, 0.0f, 1.0f)).Normalized());
		}

		ScriptQuaternion QuaternionSlerp(
			const ScriptQuaternion& start,
			const ScriptQuaternion& end,
			float amount)
		{
			return ToScript(Quat::Slerp(
				ToNative(start),
				ToNative(end),
				std::clamp(amount, 0.0f, 1.0f)).Normalized());
		}

		ScriptQuaternion QuaternionRotateTowards(
			const ScriptQuaternion& current,
			const ScriptQuaternion& target,
			float maximumRadiansDelta)
		{
			const float angle = QuaternionAngle(current, target);
			if (angle <= Epsilon)
				return NormalizeQuaternion(target);
			return QuaternionSlerp(
				current,
				target,
				std::min(1.0f, std::max(0.0f, maximumRadiansDelta) / angle));
		}

		bool RegisterVector2(asIScriptEngine& engine)
		{
			return engine.RegisterObjectType(
					"Vector2", sizeof(ScriptVector2),
					asOBJ_VALUE | asOBJ_APP_CLASS_CDAK |
						asOBJ_APP_CLASS_ALLFLOATS) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Vector2", asBEHAVE_CONSTRUCT,
					"void f(float x = 0, float y = 0)",
					asFUNCTION(ConstructVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Vector2", asBEHAVE_CONSTRUCT,
					"void f(const Vector2 &in)",
					asFUNCTION(CopyConstructVector2),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Vector2", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(DestructVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 &opAssign(const Vector2 &in)",
					asFUNCTION(AssignVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectProperty(
					"Vector2", "float x", asOFFSET(ScriptVector2, x)) >= 0 &&
				engine.RegisterObjectProperty(
					"Vector2", "float y", asOFFSET(ScriptVector2, y)) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 opAdd(const Vector2 &in) const",
					asFUNCTION(AddVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 opSub(const Vector2 &in) const",
					asFUNCTION(SubtractVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 opNeg() const",
					asFUNCTION(NegateVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 opMul(float) const",
					asFUNCTION(MultiplyVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 opMul_r(float) const",
					asFUNCTION(MultiplyVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 opDiv(float) const",
					asFUNCTION(DivideVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 &opAddAssign(const Vector2 &in)",
					asFUNCTION(AddAssignVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 &opSubAssign(const Vector2 &in)",
					asFUNCTION(SubtractAssignVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 &opMulAssign(float)",
					asFUNCTION(MultiplyAssignVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 &opDivAssign(float)",
					asFUNCTION(DivideAssignVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "bool opEquals(const Vector2 &in) const",
					asFUNCTION(EqualVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "float get_length() const property",
					asFUNCTION(LengthVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "float get_lengthSquared() const property",
					asFUNCTION(LengthSquaredVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "Vector2 get_normalized() const property",
					asFUNCTION(NormalizeVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "bool get_isFinite() const property",
					asFUNCTION(IsFiniteVector2), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector2", "void Normalize()",
					asFUNCTION(NormalizeVector2InPlace),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterQuaternion(asIScriptEngine& engine)
		{
			return engine.RegisterObjectType(
					"Quaternion", sizeof(ScriptQuaternion),
					asOBJ_VALUE |
						asGetTypeTraits<ScriptQuaternion>()) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Quaternion", asBEHAVE_CONSTRUCT,
					"void f(float x = 0, float y = 0, "
						"float z = 0, float w = 1)",
					asFUNCTION(ConstructQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Quaternion", asBEHAVE_CONSTRUCT,
					"void f(const Quaternion &in)",
					asFUNCTION(CopyConstructQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Quaternion", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(DestructQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion",
					"Quaternion &opAssign(const Quaternion &in)",
					asFUNCTION(AssignQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectProperty(
					"Quaternion", "float x",
					asOFFSET(ScriptQuaternion, x)) >= 0 &&
				engine.RegisterObjectProperty(
					"Quaternion", "float y",
					asOFFSET(ScriptQuaternion, y)) >= 0 &&
				engine.RegisterObjectProperty(
					"Quaternion", "float z",
					asOFFSET(ScriptQuaternion, z)) >= 0 &&
				engine.RegisterObjectProperty(
					"Quaternion", "float w",
					asOFFSET(ScriptQuaternion, w)) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion",
					"Quaternion opMul(const Quaternion &in) const",
					asFUNCTION(MultiplyQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion", "Vector3 opMul(const Vector3 &in) const",
					asFUNCTION(RotateVector), asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion",
					"bool opEquals(const Quaternion &in) const",
					asFUNCTION(EqualQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion", "float get_lengthSquared() const property",
					asFUNCTION(LengthSquaredQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion",
					"Quaternion get_normalized() const property",
					asFUNCTION(NormalizeQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion", "bool get_isFinite() const property",
					asFUNCTION(IsFiniteQuaternion),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion",
					"Vector3 get_eulerAngles() const property",
					asFUNCTION(GetQuaternionEulerAngles),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Quaternion", "void Normalize()",
					asFUNCTION(NormalizeQuaternionInPlace),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterExtendedMathFunctions(asIScriptEngine& engine)
		{
			engine.SetDefaultNamespace("Math");
			bool registered =
				engine.RegisterGlobalProperty(
					"const Vector2 Vector2Zero",
					const_cast<ScriptVector2*>(&Vector2Zero)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector2 Vector2One",
					const_cast<ScriptVector2*>(&Vector2One)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector2 Vector2Up",
					const_cast<ScriptVector2*>(&Vector2Up)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector2 Vector2Down",
					const_cast<ScriptVector2*>(&Vector2Down)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector2 Vector2Left",
					const_cast<ScriptVector2*>(&Vector2Left)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector2 Vector2Right",
					const_cast<ScriptVector2*>(&Vector2Right)) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Dot(const Vector2 &in left, const Vector2 &in right)",
					asFUNCTION(DotVector2), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Distance(const Vector2 &in left, const Vector2 &in right)",
					asFUNCTION(DistanceVector2), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector2 Lerp(const Vector2 &in start, const Vector2 &in end, float amount)",
					asFUNCTION(LerpVector2), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector2 LerpUnclamped(const Vector2 &in start, const Vector2 &in end, float amount)",
					asFUNCTION(LerpUnclampedVector2), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector2 MoveTowards(const Vector2 &in current, const Vector2 &in target, float maxDistanceDelta)",
					asFUNCTION(MoveTowardsVector2), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Angle(const Vector2 &in left, const Vector2 &in right)",
					asFUNCTION(AngleVector2), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			if (!registered)
				return false;

			engine.SetDefaultNamespace("Quaternion");
			registered =
				engine.RegisterGlobalProperty(
					"const Quaternion Identity",
					const_cast<ScriptQuaternion*>(&QuaternionIdentity)) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion Euler(const Vector3 &in radians)",
					asFUNCTION(QuaternionEuler), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion AngleAxis(float radians, const Vector3 &in axis)",
					asFUNCTION(QuaternionAngleAxis), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion FromToRotation(const Vector3 &in from, const Vector3 &in to)",
					asFUNCTION(QuaternionFromToRotation), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion LookRotation(const Vector3 &in forward, const Vector3 &in up = Math::Vector3Up)",
					asFUNCTION(QuaternionLookRotation), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion Inverse(const Quaternion &in value)",
					asFUNCTION(QuaternionInverse), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion Normalize(const Quaternion &in value)",
					asFUNCTION(NormalizeQuaternion), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Dot(const Quaternion &in left, const Quaternion &in right)",
					asFUNCTION(QuaternionDot), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Angle(const Quaternion &in left, const Quaternion &in right)",
					asFUNCTION(QuaternionAngle), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion Lerp(const Quaternion &in start, const Quaternion &in end, float amount)",
					asFUNCTION(QuaternionLerp), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion Slerp(const Quaternion &in start, const Quaternion &in end, float amount)",
					asFUNCTION(QuaternionSlerp), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Quaternion RotateTowards(const Quaternion &in current, const Quaternion &in target, float maximumRadiansDelta)",
					asFUNCTION(QuaternionRotateTowards), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return registered;
		}
	}

	bool RegisterExtendedMathApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		if (!RegisterVector2(engine))
		{
			error = "Could not register the Vector2 API.";
			return false;
		}
		if (!RegisterQuaternion(engine))
		{
			error = "Could not register the Quaternion API.";
			return false;
		}
		if (!RegisterExtendedMathFunctions(engine))
		{
			error = "Could not register the extended Math API.";
			return false;
		}
		return true;
	}
}
