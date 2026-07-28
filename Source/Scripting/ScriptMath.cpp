#include "ScriptMath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>

namespace EGE
{
	namespace
	{
		constexpr float Pi = 3.14159265358979323846f;
		constexpr float DegreesToRadians = Pi / 180.0f;
		constexpr float RadiansToDegrees = 180.0f / Pi;
		constexpr float Epsilon = 0.00001f;

		const ScriptVector3 Vector3Zero{};
		const ScriptVector3 Vector3One{1.0f, 1.0f, 1.0f};
		const ScriptVector3 Vector3Up{0.0f, 1.0f, 0.0f};
		const ScriptVector3 Vector3Down{0.0f, -1.0f, 0.0f};
		const ScriptVector3 Vector3Left{-1.0f, 0.0f, 0.0f};
		const ScriptVector3 Vector3Right{1.0f, 0.0f, 0.0f};
		const ScriptVector3 Vector3Forward{0.0f, 0.0f, 1.0f};
		const ScriptVector3 Vector3Back{0.0f, 0.0f, -1.0f};

		const ScriptColor ColorClear{0.0f, 0.0f, 0.0f, 0.0f};
		const ScriptColor ColorBlack{0.0f, 0.0f, 0.0f, 1.0f};
		const ScriptColor ColorWhite{1.0f, 1.0f, 1.0f, 1.0f};
		const ScriptColor ColorRed{1.0f, 0.0f, 0.0f, 1.0f};
		const ScriptColor ColorGreen{0.0f, 1.0f, 0.0f, 1.0f};
		const ScriptColor ColorBlue{0.0f, 0.0f, 1.0f, 1.0f};
		const ScriptColor ColorYellow{1.0f, 1.0f, 0.0f, 1.0f};
		const ScriptColor ColorCyan{0.0f, 1.0f, 1.0f, 1.0f};
		const ScriptColor ColorMagenta{1.0f, 0.0f, 1.0f, 1.0f};

		void ConstructVector3(
			float x,
			float y,
			float z,
			ScriptVector3* value)
		{
			new (value) ScriptVector3{x, y, z};
		}

		void CopyConstructVector3(
			const ScriptVector3& source,
			ScriptVector3* value)
		{
			new (value) ScriptVector3(source);
		}

		void DestructVector3(ScriptVector3* value)
		{
			value->~ScriptVector3();
		}

		ScriptVector3& AssignVector3(
			const ScriptVector3& source,
			ScriptVector3* value)
		{
			*value = source;
			return *value;
		}

		ScriptVector3 AddVector3(
			const ScriptVector3& right,
			const ScriptVector3& left)
		{
			return {
				left.x + right.x,
				left.y + right.y,
				left.z + right.z};
		}

		ScriptVector3 SubtractVector3(
			const ScriptVector3& right,
			const ScriptVector3& left)
		{
			return {
				left.x - right.x,
				left.y - right.y,
				left.z - right.z};
		}

		ScriptVector3 NegateVector3(const ScriptVector3& value)
		{
			return {-value.x, -value.y, -value.z};
		}

		ScriptVector3 MultiplyVector3(
			float scalar,
			const ScriptVector3& value)
		{
			return {value.x * scalar, value.y * scalar, value.z * scalar};
		}

		ScriptVector3 DivideVector3(
			float scalar,
			const ScriptVector3& value)
		{
			return std::abs(scalar) > Epsilon
				? ScriptVector3{
					value.x / scalar,
					value.y / scalar,
					value.z / scalar}
				: ScriptVector3{};
		}

		ScriptVector3& AddAssignVector3(
			const ScriptVector3& right,
			ScriptVector3* left)
		{
			left->x += right.x;
			left->y += right.y;
			left->z += right.z;
			return *left;
		}

		ScriptVector3& SubtractAssignVector3(
			const ScriptVector3& right,
			ScriptVector3* left)
		{
			left->x -= right.x;
			left->y -= right.y;
			left->z -= right.z;
			return *left;
		}

		ScriptVector3& MultiplyAssignVector3(
			float scalar,
			ScriptVector3* value)
		{
			value->x *= scalar;
			value->y *= scalar;
			value->z *= scalar;
			return *value;
		}

		ScriptVector3& DivideAssignVector3(
			float scalar,
			ScriptVector3* value)
		{
			*value = DivideVector3(scalar, *value);
			return *value;
		}

		bool EqualVector3(
			const ScriptVector3& right,
			const ScriptVector3& left)
		{
			return left.x == right.x &&
				left.y == right.y &&
				left.z == right.z;
		}

		float DotVector3(
			const ScriptVector3& left,
			const ScriptVector3& right)
		{
			return left.x * right.x +
				left.y * right.y +
				left.z * right.z;
		}

		float LengthSquaredVector3(const ScriptVector3& value)
		{
			return DotVector3(value, value);
		}

		float LengthVector3(const ScriptVector3& value)
		{
			return std::sqrt(LengthSquaredVector3(value));
		}

		ScriptVector3 NormalizeVector3(const ScriptVector3& value)
		{
			const float length = LengthVector3(value);
			return length > Epsilon
				? DivideVector3(length, value)
				: ScriptVector3{};
		}

		void NormalizeVector3InPlace(ScriptVector3* value)
		{
			*value = NormalizeVector3(*value);
		}

		bool IsFiniteVector3(const ScriptVector3& value)
		{
			return std::isfinite(value.x) &&
				std::isfinite(value.y) &&
				std::isfinite(value.z);
		}

		ScriptVector3 CrossVector3(
			const ScriptVector3& left,
			const ScriptVector3& right)
		{
			return {
				left.y * right.z - left.z * right.y,
				left.z * right.x - left.x * right.z,
				left.x * right.y - left.y * right.x};
		}

		float DistanceVector3(
			const ScriptVector3& left,
			const ScriptVector3& right)
		{
			return LengthVector3(SubtractVector3(right, left));
		}

		ScriptVector3 LerpVector3(
			const ScriptVector3& start,
			const ScriptVector3& end,
			float amount)
		{
			const float clamped = std::clamp(amount, 0.0f, 1.0f);
			return AddVector3(
				MultiplyVector3(clamped, SubtractVector3(start, end)),
				start);
		}

		ScriptVector3 LerpUnclampedVector3(
			const ScriptVector3& start,
			const ScriptVector3& end,
			float amount)
		{
			return AddVector3(
				MultiplyVector3(amount, SubtractVector3(start, end)),
				start);
		}

		ScriptVector3 MoveTowardsVector3(
			const ScriptVector3& current,
			const ScriptVector3& target,
			float maxDistanceDelta)
		{
			const ScriptVector3 delta = SubtractVector3(current, target);
			const float distance = LengthVector3(delta);
			if (distance <= maxDistanceDelta || distance <= Epsilon)
				return target;
			return AddVector3(
				MultiplyVector3(maxDistanceDelta / distance, delta),
				current);
		}

		ScriptVector3 ReflectVector3(
			const ScriptVector3& direction,
			const ScriptVector3& normal)
		{
			return SubtractVector3(
				MultiplyVector3(2.0f * DotVector3(direction, normal), normal),
				direction);
		}

		ScriptVector3 ProjectVector3(
			const ScriptVector3& vector,
			const ScriptVector3& onNormal)
		{
			const float denominator = LengthSquaredVector3(onNormal);
			return denominator > Epsilon
				? MultiplyVector3(
					DotVector3(vector, onNormal) / denominator,
					onNormal)
				: ScriptVector3{};
		}

		ScriptVector3 MinVector3(
			const ScriptVector3& left,
			const ScriptVector3& right)
		{
			return {
				std::min(left.x, right.x),
				std::min(left.y, right.y),
				std::min(left.z, right.z)};
		}

		ScriptVector3 MaxVector3(
			const ScriptVector3& left,
			const ScriptVector3& right)
		{
			return {
				std::max(left.x, right.x),
				std::max(left.y, right.y),
				std::max(left.z, right.z)};
		}

		ScriptVector3 ClampMagnitudeVector3(
			const ScriptVector3& value,
			float maximumLength)
		{
			const float safeMaximum = std::max(0.0f, maximumLength);
			const float length = LengthVector3(value);
			return length > safeMaximum && length > Epsilon
				? MultiplyVector3(safeMaximum / length, value)
				: value;
		}

		float AngleVector3(
			const ScriptVector3& left,
			const ScriptVector3& right)
		{
			const float denominator = std::sqrt(
				LengthSquaredVector3(left) *
				LengthSquaredVector3(right));
			if (denominator <= Epsilon)
				return 0.0f;
			const float cosine = std::clamp(
				DotVector3(left, right) / denominator,
				-1.0f,
				1.0f);
			return std::acos(cosine) * RadiansToDegrees;
		}

		void ConstructColor(
			float r,
			float g,
			float b,
			float a,
			ScriptColor* value)
		{
			new (value) ScriptColor{r, g, b, a};
		}

		void CopyConstructColor(
			const ScriptColor& source,
			ScriptColor* value)
		{
			new (value) ScriptColor(source);
		}

		void DestructColor(ScriptColor* value)
		{
			value->~ScriptColor();
		}

		ScriptColor& AssignColor(
			const ScriptColor& source,
			ScriptColor* value)
		{
			*value = source;
			return *value;
		}

		ScriptColor AddColor(
			const ScriptColor& right,
			const ScriptColor& left)
		{
			return {
				left.r + right.r,
				left.g + right.g,
				left.b + right.b,
				left.a + right.a};
		}

		ScriptColor SubtractColor(
			const ScriptColor& right,
			const ScriptColor& left)
		{
			return {
				left.r - right.r,
				left.g - right.g,
				left.b - right.b,
				left.a - right.a};
		}

		ScriptColor MultiplyColors(
			const ScriptColor& right,
			const ScriptColor& left)
		{
			return {
				left.r * right.r,
				left.g * right.g,
				left.b * right.b,
				left.a * right.a};
		}

		ScriptColor MultiplyColor(
			float scalar,
			const ScriptColor& value)
		{
			return {
				value.r * scalar,
				value.g * scalar,
				value.b * scalar,
				value.a * scalar};
		}

		ScriptColor DivideColor(
			float scalar,
			const ScriptColor& value)
		{
			return std::abs(scalar) > Epsilon
				? ScriptColor{
					value.r / scalar,
					value.g / scalar,
					value.b / scalar,
					value.a / scalar}
				: ScriptColor{};
		}

		ScriptColor& AddAssignColor(
			const ScriptColor& right,
			ScriptColor* left)
		{
			*left = AddColor(right, *left);
			return *left;
		}

		ScriptColor& SubtractAssignColor(
			const ScriptColor& right,
			ScriptColor* left)
		{
			*left = SubtractColor(right, *left);
			return *left;
		}

		ScriptColor& MultiplyAssignColor(
			float scalar,
			ScriptColor* value)
		{
			*value = MultiplyColor(scalar, *value);
			return *value;
		}

		ScriptColor& DivideAssignColor(
			float scalar,
			ScriptColor* value)
		{
			*value = DivideColor(scalar, *value);
			return *value;
		}

		bool EqualColor(
			const ScriptColor& right,
			const ScriptColor& left)
		{
			return left.r == right.r &&
				left.g == right.g &&
				left.b == right.b &&
				left.a == right.a;
		}

		float GrayscaleColor(const ScriptColor& value)
		{
			return value.r * 0.299f +
				value.g * 0.587f +
				value.b * 0.114f;
		}

		ScriptColor LerpColor(
			const ScriptColor& start,
			const ScriptColor& end,
			float amount)
		{
			const float clamped = std::clamp(amount, 0.0f, 1.0f);
			return {
				std::lerp(start.r, end.r, clamped),
				std::lerp(start.g, end.g, clamped),
				std::lerp(start.b, end.b, clamped),
				std::lerp(start.a, end.a, clamped)};
		}

		ScriptColor LerpUnclampedColor(
			const ScriptColor& start,
			const ScriptColor& end,
			float amount)
		{
			return {
				std::lerp(start.r, end.r, amount),
				std::lerp(start.g, end.g, amount),
				std::lerp(start.b, end.b, amount),
				std::lerp(start.a, end.a, amount)};
		}

		ScriptColor ClampColor(const ScriptColor& value)
		{
			return {
				std::clamp(value.r, 0.0f, 1.0f),
				std::clamp(value.g, 0.0f, 1.0f),
				std::clamp(value.b, 0.0f, 1.0f),
				std::clamp(value.a, 0.0f, 1.0f)};
		}

		int AbsInt(int value)
		{
			return value == std::numeric_limits<int>::min()
				? std::numeric_limits<int>::max()
				: std::abs(value);
		}

		float AbsFloat(float value) { return std::abs(value); }
		float MinFloat(float left, float right) { return std::min(left, right); }
		float MaxFloat(float left, float right) { return std::max(left, right); }
		int MinInt(int left, int right) { return std::min(left, right); }
		int MaxInt(int left, int right) { return std::max(left, right); }
		float ClampFloat(float value, float minimum, float maximum)
		{
			if (minimum > maximum)
				std::swap(minimum, maximum);
			return std::clamp(value, minimum, maximum);
		}
		int ClampInt(int value, int minimum, int maximum)
		{
			if (minimum > maximum)
				std::swap(minimum, maximum);
			return std::clamp(value, minimum, maximum);
		}
		float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
		float Sign(float value)
		{
			return value > 0.0f ? 1.0f : value < 0.0f ? -1.0f : 0.0f;
		}
		float Sqrt(float value) { return std::sqrt(std::max(0.0f, value)); }
		float Pow(float value, float power) { return std::pow(value, power); }
		float Exp(float value) { return std::exp(value); }
		float Log(float value) { return std::log(value); }
		float Log10(float value) { return std::log10(value); }
		float Sin(float value) { return std::sin(value); }
		float Cos(float value) { return std::cos(value); }
		float Tan(float value) { return std::tan(value); }
		float Asin(float value)
		{
			return std::asin(std::clamp(value, -1.0f, 1.0f));
		}
		float Acos(float value)
		{
			return std::acos(std::clamp(value, -1.0f, 1.0f));
		}
		float Atan(float value) { return std::atan(value); }
		float Atan2(float y, float x) { return std::atan2(y, x); }
		float Floor(float value) { return std::floor(value); }
		float Ceil(float value) { return std::ceil(value); }
		float Round(float value) { return std::round(value); }
		float Lerp(float start, float end, float amount)
		{
			return std::lerp(start, end, Clamp01(amount));
		}
		float LerpUnclamped(float start, float end, float amount)
		{
			return std::lerp(start, end, amount);
		}
		float InverseLerp(float start, float end, float value)
		{
			const float range = end - start;
			return std::abs(range) <= Epsilon
				? 0.0f
				: Clamp01((value - start) / range);
		}
		float MoveTowards(float current, float target, float maxDelta)
		{
			if (std::abs(target - current) <= maxDelta)
				return target;
			return current + Sign(target - current) * maxDelta;
		}
		float Repeat(float value, float length)
		{
			if (length <= Epsilon)
				return 0.0f;
			return std::clamp(
				value - std::floor(value / length) * length,
				0.0f,
				length);
		}
		float PingPong(float value, float length)
		{
			const float repeated = Repeat(value, length * 2.0f);
			return length - std::abs(repeated - length);
		}
		bool Approximately(float left, float right)
		{
			return std::abs(left - right) <=
				std::max(
					0.000001f * std::max(std::abs(left), std::abs(right)),
					Epsilon * 8.0f);
		}

		bool RegisterVector3(asIScriptEngine& engine)
		{
			return
				engine.RegisterObjectType(
					"Vector3", sizeof(ScriptVector3),
					asOBJ_VALUE | asOBJ_APP_CLASS_CDAK |
						asOBJ_APP_CLASS_ALLFLOATS) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Vector3", asBEHAVE_CONSTRUCT,
					"void f(float x = 0, float y = 0, float z = 0)",
					asFUNCTION(ConstructVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Vector3", asBEHAVE_CONSTRUCT,
					"void f(const Vector3 &in)",
					asFUNCTION(CopyConstructVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Vector3", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(DestructVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 &opAssign(const Vector3 &in)",
					asFUNCTION(AssignVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectProperty(
					"Vector3", "float x",
					asOFFSET(ScriptVector3, x)) >= 0 &&
				engine.RegisterObjectProperty(
					"Vector3", "float y",
					asOFFSET(ScriptVector3, y)) >= 0 &&
				engine.RegisterObjectProperty(
					"Vector3", "float z",
					asOFFSET(ScriptVector3, z)) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 opAdd(const Vector3 &in) const",
					asFUNCTION(AddVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 opSub(const Vector3 &in) const",
					asFUNCTION(SubtractVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 opNeg() const",
					asFUNCTION(NegateVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 opMul(float) const",
					asFUNCTION(MultiplyVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 opMul_r(float) const",
					asFUNCTION(MultiplyVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 opDiv(float) const",
					asFUNCTION(DivideVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 &opAddAssign(const Vector3 &in)",
					asFUNCTION(AddAssignVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 &opSubAssign(const Vector3 &in)",
					asFUNCTION(SubtractAssignVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 &opMulAssign(float)",
					asFUNCTION(MultiplyAssignVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 &opDivAssign(float)",
					asFUNCTION(DivideAssignVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "bool opEquals(const Vector3 &in) const",
					asFUNCTION(EqualVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "float get_length() const property",
					asFUNCTION(LengthVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "float get_lengthSquared() const property",
					asFUNCTION(LengthSquaredVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "Vector3 get_normalized() const property",
					asFUNCTION(NormalizeVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "bool get_isFinite() const property",
					asFUNCTION(IsFiniteVector3),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Vector3", "void Normalize()",
					asFUNCTION(NormalizeVector3InPlace),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterColor(asIScriptEngine& engine)
		{
			return
				engine.RegisterObjectType(
					"Color", sizeof(ScriptColor),
					asOBJ_VALUE | asOBJ_APP_CLASS_CDAK |
						asOBJ_APP_CLASS_ALLFLOATS) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Color", asBEHAVE_CONSTRUCT,
					"void f(float r = 0, float g = 0, "
						"float b = 0, float a = 1)",
					asFUNCTION(ConstructColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Color", asBEHAVE_CONSTRUCT,
					"void f(const Color &in)",
					asFUNCTION(CopyConstructColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectBehaviour(
					"Color", asBEHAVE_DESTRUCT, "void f()",
					asFUNCTION(DestructColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color &opAssign(const Color &in)",
					asFUNCTION(AssignColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectProperty(
					"Color", "float r", asOFFSET(ScriptColor, r)) >= 0 &&
				engine.RegisterObjectProperty(
					"Color", "float g", asOFFSET(ScriptColor, g)) >= 0 &&
				engine.RegisterObjectProperty(
					"Color", "float b", asOFFSET(ScriptColor, b)) >= 0 &&
				engine.RegisterObjectProperty(
					"Color", "float a", asOFFSET(ScriptColor, a)) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color opAdd(const Color &in) const",
					asFUNCTION(AddColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color opSub(const Color &in) const",
					asFUNCTION(SubtractColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color opMul(const Color &in) const",
					asFUNCTION(MultiplyColors),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color opMul(float) const",
					asFUNCTION(MultiplyColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color opMul_r(float) const",
					asFUNCTION(MultiplyColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color opDiv(float) const",
					asFUNCTION(DivideColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color &opAddAssign(const Color &in)",
					asFUNCTION(AddAssignColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color &opSubAssign(const Color &in)",
					asFUNCTION(SubtractAssignColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color &opMulAssign(float)",
					asFUNCTION(MultiplyAssignColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "Color &opDivAssign(float)",
					asFUNCTION(DivideAssignColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "bool opEquals(const Color &in) const",
					asFUNCTION(EqualColor),
					asCALL_CDECL_OBJLAST) >= 0 &&
				engine.RegisterObjectMethod(
					"Color", "float get_grayscale() const property",
					asFUNCTION(GrayscaleColor),
					asCALL_CDECL_OBJLAST) >= 0;
		}

		bool RegisterMathFunctions(asIScriptEngine& engine)
		{
			engine.SetDefaultNamespace("Math");
			const bool registered =
				engine.RegisterGlobalProperty(
					"const float Pi", const_cast<float*>(&Pi)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const float Deg2Rad",
					const_cast<float*>(&DegreesToRadians)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const float Rad2Deg",
					const_cast<float*>(&RadiansToDegrees)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const float Epsilon",
					const_cast<float*>(&Epsilon)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Zero",
					const_cast<ScriptVector3*>(&Vector3Zero)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3One",
					const_cast<ScriptVector3*>(&Vector3One)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Up",
					const_cast<ScriptVector3*>(&Vector3Up)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Down",
					const_cast<ScriptVector3*>(&Vector3Down)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Left",
					const_cast<ScriptVector3*>(&Vector3Left)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Right",
					const_cast<ScriptVector3*>(&Vector3Right)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Forward",
					const_cast<ScriptVector3*>(&Vector3Forward)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Vector3 Vector3Back",
					const_cast<ScriptVector3*>(&Vector3Back)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorClear",
					const_cast<ScriptColor*>(&ColorClear)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorBlack",
					const_cast<ScriptColor*>(&ColorBlack)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorWhite",
					const_cast<ScriptColor*>(&ColorWhite)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorRed",
					const_cast<ScriptColor*>(&ColorRed)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorGreen",
					const_cast<ScriptColor*>(&ColorGreen)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorBlue",
					const_cast<ScriptColor*>(&ColorBlue)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorYellow",
					const_cast<ScriptColor*>(&ColorYellow)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorCyan",
					const_cast<ScriptColor*>(&ColorCyan)) >= 0 &&
				engine.RegisterGlobalProperty(
					"const Color ColorMagenta",
					const_cast<ScriptColor*>(&ColorMagenta)) >= 0 &&
				engine.RegisterGlobalFunction(
					"int Abs(int value)",
					asFUNCTION(AbsInt), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Abs(float value)",
					asFUNCTION(AbsFloat), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"int Min(int left, int right)",
					asFUNCTION(MinInt), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Min(float left, float right)",
					asFUNCTION(MinFloat), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"int Max(int left, int right)",
					asFUNCTION(MaxInt), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Max(float left, float right)",
					asFUNCTION(MaxFloat), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"int Clamp(int value, int minimum, int maximum)",
					asFUNCTION(ClampInt), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Clamp(float value, float minimum, float maximum)",
					asFUNCTION(ClampFloat), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Clamp01(float value)",
					asFUNCTION(Clamp01), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Sign(float value)",
					asFUNCTION(Sign), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Sqrt(float value)",
					asFUNCTION(Sqrt), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Pow(float value, float power)",
					asFUNCTION(Pow), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Exp(float value)",
					asFUNCTION(Exp), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Log(float value)",
					asFUNCTION(Log), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Log10(float value)",
					asFUNCTION(Log10), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Sin(float radians)",
					asFUNCTION(Sin), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Cos(float radians)",
					asFUNCTION(Cos), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Tan(float radians)",
					asFUNCTION(Tan), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Asin(float value)",
					asFUNCTION(Asin), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Acos(float value)",
					asFUNCTION(Acos), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Atan(float value)",
					asFUNCTION(Atan), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Atan2(float y, float x)",
					asFUNCTION(Atan2), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Floor(float value)",
					asFUNCTION(Floor), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Ceil(float value)",
					asFUNCTION(Ceil), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Round(float value)",
					asFUNCTION(Round), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Lerp(float start, float end, float amount)",
					asFUNCTION(Lerp), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float LerpUnclamped(float start, float end, float amount)",
					asFUNCTION(LerpUnclamped), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float InverseLerp(float start, float end, float value)",
					asFUNCTION(InverseLerp), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float MoveTowards(float current, float target, float maxDelta)",
					asFUNCTION(MoveTowards), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Repeat(float value, float length)",
					asFUNCTION(Repeat), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float PingPong(float value, float length)",
					asFUNCTION(PingPong), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"bool Approximately(float left, float right)",
					asFUNCTION(Approximately), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Dot(const Vector3 &in left, const Vector3 &in right)",
					asFUNCTION(DotVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 Cross(const Vector3 &in left, const Vector3 &in right)",
					asFUNCTION(CrossVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Distance(const Vector3 &in left, const Vector3 &in right)",
					asFUNCTION(DistanceVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 Lerp(const Vector3 &in start, const Vector3 &in end, float amount)",
					asFUNCTION(LerpVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 LerpUnclamped(const Vector3 &in start, const Vector3 &in end, float amount)",
					asFUNCTION(LerpUnclampedVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 MoveTowards(const Vector3 &in current, const Vector3 &in target, float maxDistanceDelta)",
					asFUNCTION(MoveTowardsVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 Reflect(const Vector3 &in direction, const Vector3 &in normal)",
					asFUNCTION(ReflectVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 Project(const Vector3 &in vector, const Vector3 &in onNormal)",
					asFUNCTION(ProjectVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 Min(const Vector3 &in left, const Vector3 &in right)",
					asFUNCTION(MinVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 Max(const Vector3 &in left, const Vector3 &in right)",
					asFUNCTION(MaxVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Vector3 ClampMagnitude(const Vector3 &in value, float maximumLength)",
					asFUNCTION(ClampMagnitudeVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"float Angle(const Vector3 &in left, const Vector3 &in right)",
					asFUNCTION(AngleVector3), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Color Lerp(const Color &in start, const Color &in end, float amount)",
					asFUNCTION(LerpColor), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Color LerpUnclamped(const Color &in start, const Color &in end, float amount)",
					asFUNCTION(LerpUnclampedColor), asCALL_CDECL) >= 0 &&
				engine.RegisterGlobalFunction(
					"Color Clamp01(const Color &in value)",
					asFUNCTION(ClampColor), asCALL_CDECL) >= 0;
			engine.SetDefaultNamespace("");
			return registered;
		}
	}

	bool RegisterMathApi(
		asIScriptEngine& engine,
		std::string& error)
	{
		if (!RegisterVector3(engine))
		{
			error = "Could not register the Vector3 API.";
			return false;
		}
		if (!RegisterColor(engine))
		{
			error = "Could not register the Color API.";
			return false;
		}
		if (!RegisterMathFunctions(engine))
		{
			error = "Could not register the Math API.";
			return false;
		}
		return true;
	}
}
