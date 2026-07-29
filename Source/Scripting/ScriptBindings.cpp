#include "ScriptBindings.h"

#include "../Application.h"
#include "../Component.h"
#include "../GameObject.h"
#include "../Math.h"
#include "../ModuleInput.h"
#include "../ModuleLevelManager.h"
#include "../Globals.h"
#include "ScriptMath.h"
#include "ScriptPhysics.h"
#include "ScriptPhysicsQueries.h"
#include "ScriptCoreHelpers.h"
#include "ScriptComponentBindings.h"
#include "ScriptDebugDraw.h"
#include "ScriptObjectReference.h"
#include "ScriptSceneBindings.h"

#include <angelscript.h>
#include <SDL_mouse.h>
#include <SDL_scancode.h>
#include <scriptarray/scriptarray.h>
#include <scriptstdstring/scriptstdstring.h>

#include <charconv>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>

// =============================================================================
// Input wrapper függvények
// =============================================================================
namespace EGE
{
namespace {

void SelfTestMessageCallback(
	const asSMessageInfo* message,
	void*)
{
	if (!message || message->type == asMSGTYPE_INFORMATION)
		return;
	std::fprintf(
		stderr,
		"%s:%d:%d %s\n",
		message->section ? message->section : "script",
		message->row,
		message->col,
		message->message ? message->message : "AngelScript error");
}

ScriptVector3 ToScriptVector3(const float3& value)
{
	return {value.x, value.y, value.z};
}

float3 ToEngineVector3(const ScriptVector3& value)
{
	return {value.x, value.y, value.z};
}

ScriptQuaternion ToScriptQuaternion(const Quat& value)
{
	return {value.x, value.y, value.z, value.w};
}

Quat ToEngineQuaternion(const ScriptQuaternion& value)
{
	return Quat(value.x, value.y, value.z, value.w).Normalized();
}

std::string Trim(std::string_view value)
{
	while (!value.empty() &&
		std::isspace(static_cast<unsigned char>(value.front())))
	{
		value.remove_prefix(1);
	}
	while (!value.empty() &&
		std::isspace(static_cast<unsigned char>(value.back())))
	{
		value.remove_suffix(1);
	}
	return std::string(value);
}

std::string Lowercase(std::string value)
{
	for (char& character : value)
	{
		character = static_cast<char>(
			std::tolower(static_cast<unsigned char>(character)));
	}
	return value;
}

bool TryParseInt(const std::string& value, int& result)
{
	const std::string text = Trim(value);
	if (text.empty())
		return false;
	const auto [end, error] = std::from_chars(
		text.data(), text.data() + text.size(), result);
	return error == std::errc{} && end == text.data() + text.size();
}

bool TryParseUInt(const std::string& value, unsigned int& result)
{
	const std::string text = Trim(value);
	if (text.empty())
		return false;
	const auto [end, error] = std::from_chars(
		text.data(), text.data() + text.size(), result);
	return error == std::errc{} && end == text.data() + text.size();
}

bool TryParseDouble(const std::string& value, double& result)
{
	const std::string text = Trim(value);
	if (text.empty())
		return false;
	const auto [end, error] = std::from_chars(
		text.data(), text.data() + text.size(), result,
		std::chars_format::general);
	return error == std::errc{} && end == text.data() + text.size();
}

bool TryParseFloat(const std::string& value, float& result)
{
	double parsed = 0.0;
	if (!TryParseDouble(value, parsed))
		return false;
	result = static_cast<float>(parsed);
	return true;
}

bool TryParseBool(const std::string& value, bool& result)
{
	const std::string text = Lowercase(Trim(value));
	if (text == "true" || text == "1" || text == "yes" || text == "on")
	{
		result = true;
		return true;
	}
	if (text == "false" || text == "0" || text == "no" || text == "off")
	{
		result = false;
		return true;
	}
	return false;
}

std::string FormatDouble(double value)
{
	std::ostringstream stream;
	stream << std::setprecision(15) << value;
	return stream.str();
}

std::string ConvertToString(int value)
{
	return std::to_string(value);
}

std::string ConvertToString(unsigned int value)
{
	return std::to_string(value);
}

std::string ConvertToString(double value)
{
	return FormatDouble(value);
}

std::string ConvertToString(float value)
{
	return FormatDouble(value);
}

std::string ConvertToString(bool value)
{
	return value ? "true" : "false";
}

std::string ConvertToString(const ScriptVector3& value)
{
	return "(" + FormatDouble(value.x) + ", " +
		FormatDouble(value.y) + ", " + FormatDouble(value.z) + ")";
}

std::string ConvertToString(const ScriptVector2& value)
{
	return "(" + FormatDouble(value.x) + ", " +
		FormatDouble(value.y) + ")";
}

std::string ConvertToString(const ScriptQuaternion& value)
{
	return "(" + FormatDouble(value.x) + ", " +
		FormatDouble(value.y) + ", " + FormatDouble(value.z) + ", " +
		FormatDouble(value.w) + ")";
}

std::string ConvertToString(const ScriptColor& value)
{
	return "(" + FormatDouble(value.r) + ", " +
		FormatDouble(value.g) + ", " + FormatDouble(value.b) + ", " +
		FormatDouble(value.a) + ")";
}

bool TryParseVector3(const std::string& value, ScriptVector3& result)
{
	std::string text = Trim(value);
	if (text.size() >= 2 &&
		((text.front() == '(' && text.back() == ')') ||
			(text.front() == '[' && text.back() == ']')))
	{
		text = text.substr(1, text.size() - 2);
	}

	const std::size_t first = text.find(',');
	const std::size_t second = first == std::string::npos
		? std::string::npos
		: text.find(',', first + 1);
	if (first == std::string::npos || second == std::string::npos ||
		text.find(',', second + 1) != std::string::npos)
	{
		return false;
	}

	ScriptVector3 parsed;
	return TryParseFloat(text.substr(0, first), parsed.x) &&
		TryParseFloat(text.substr(first + 1, second - first - 1), parsed.y) &&
		TryParseFloat(text.substr(second + 1), parsed.z) &&
		(result = parsed, true);
}

bool TryParseVector2(const std::string& value, ScriptVector2& result)
{
	std::string text = Trim(value);
	if (text.size() >= 2 &&
		((text.front() == '(' && text.back() == ')') ||
			(text.front() == '[' && text.back() == ']')))
	{
		text = text.substr(1, text.size() - 2);
	}
	const std::size_t separator = text.find(',');
	if (separator == std::string::npos ||
		text.find(',', separator + 1) != std::string::npos)
	{
		return false;
	}
	ScriptVector2 parsed;
	return TryParseFloat(text.substr(0, separator), parsed.x) &&
		TryParseFloat(text.substr(separator + 1), parsed.y) &&
		(result = parsed, true);
}

bool TryParseQuaternion(
	const std::string& value,
	ScriptQuaternion& result)
{
	std::string text = Trim(value);
	if (text.size() >= 2 &&
		((text.front() == '(' && text.back() == ')') ||
			(text.front() == '[' && text.back() == ']')))
	{
		text = text.substr(1, text.size() - 2);
	}
	float components[4] = {};
	std::size_t begin = 0;
	for (int index = 0; index < 4; ++index)
	{
		const std::size_t separator = text.find(',', begin);
		if (index < 3 && separator == std::string::npos)
			return false;
		if (index == 3 && separator != std::string::npos)
			return false;
		const std::size_t end =
			separator == std::string::npos ? text.size() : separator;
		if (!TryParseFloat(
				text.substr(begin, end - begin), components[index]))
		{
			return false;
		}
		begin = end + 1;
	}
	Quat rotation{
		components[0], components[1], components[2], components[3]};
	if (rotation.LengthSq() <= 0.0000001f)
		return false;
	rotation.Normalize();
	result = {
		rotation.x, rotation.y, rotation.z, rotation.w};
	return true;
}

bool TryParseColor(const std::string& value, ScriptColor& result)
{
	std::string text = Trim(value);
	if (text.size() >= 2 &&
		((text.front() == '(' && text.back() == ')') ||
			(text.front() == '[' && text.back() == ']')))
	{
		text = text.substr(1, text.size() - 2);
	}

	float components[4] = {};
	std::size_t begin = 0;
	int count = 0;
	bool hasExtraComponent = false;
	while (begin <= text.size() && count < 4)
	{
		const std::size_t separator = text.find(',', begin);
		const std::size_t end = separator == std::string::npos
			? text.size()
			: separator;
		if (!TryParseFloat(text.substr(begin, end - begin), components[count]))
			return false;
		++count;
		if (separator == std::string::npos)
			break;
		if (count == 4)
		{
			hasExtraComponent = true;
			break;
		}
		begin = separator + 1;
	}

	if ((count != 3 && count != 4) || hasExtraComponent)
	{
		return false;
	}

	result = {
		components[0],
		components[1],
		components[2],
		count == 4 ? components[3] : 1.0f};
	return true;
}

int ConvertToInt(const std::string& value, int fallback)
{
	int result = fallback;
	TryParseInt(value, result);
	return result;
}

unsigned int ConvertToUInt(const std::string& value, unsigned int fallback)
{
	unsigned int result = fallback;
	TryParseUInt(value, result);
	return result;
}

double ConvertToDouble(const std::string& value, double fallback)
{
	double result = fallback;
	TryParseDouble(value, result);
	return result;
}

float ConvertToFloat(const std::string& value, float fallback)
{
	float result = fallback;
	TryParseFloat(value, result);
	return result;
}

bool ConvertToBool(const std::string& value, bool fallback)
{
	bool result = fallback;
	TryParseBool(value, result);
	return result;
}

ScriptVector3 ConvertToVector3(
	const std::string& value,
	const ScriptVector3& fallback)
{
	ScriptVector3 result = fallback;
	TryParseVector3(value, result);
	return result;
}

ScriptVector3 ConvertToVector3(const std::string& value)
{
	return ConvertToVector3(value, {});
}

ScriptVector2 ConvertToVector2(
	const std::string& value,
	const ScriptVector2& fallback)
{
	ScriptVector2 result = fallback;
	TryParseVector2(value, result);
	return result;
}

ScriptVector2 ConvertToVector2(const std::string& value)
{
	return ConvertToVector2(value, {});
}

ScriptQuaternion ConvertToQuaternion(
	const std::string& value,
	const ScriptQuaternion& fallback)
{
	ScriptQuaternion result = fallback;
	TryParseQuaternion(value, result);
	return result;
}

ScriptQuaternion ConvertToQuaternion(const std::string& value)
{
	return ConvertToQuaternion(value, {});
}

ScriptColor ConvertToColor(
	const std::string& value,
	const ScriptColor& fallback)
{
	ScriptColor result = fallback;
	TryParseColor(value, result);
	return result;
}

ScriptColor ConvertToColor(const std::string& value)
{
	return ConvertToColor(value, {});
}

GameObject* ResolveGameObject(
	const ScriptGameObjectReference* reference,
	bool reportInvalid = true)
{
	GameObject* gameObject =
		reference ? reference->Resolve() : nullptr;
	if (!gameObject && reference && reportInvalid)
	{
		if (asIScriptContext* context = asGetActiveContext())
		{
			context->SetException(
				"The GameObject reference is no longer valid.");
		}
	}
	return gameObject;
}

Component* ResolveComponent(
	const ScriptComponentReference* reference,
	bool reportInvalid = true)
{
	Component* component =
		reference ? reference->Resolve() : nullptr;
	if (!component && reference && reportInvalid)
	{
		if (asIScriptContext* context = asGetActiveContext())
		{
			context->SetException(
				"The Component reference is no longer valid.");
		}
	}
	return component;
}

std::string ConvertToString(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject =
		ResolveGameObject(reference, false);
	if (!gameObject)
		return "<null GameObject>";
	return gameObject->name + " (" +
		std::to_string(gameObject->GetUID()) + ")";
}

std::string ConvertTransformToString(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject =
		ResolveGameObject(reference, false);
	if (!gameObject)
		return "<null Transform>";
	return ConvertToString(
		ToScriptVector3(gameObject->GetGlobalPosition()));
}

std::string ConvertComponentToString(
	const ScriptComponentReference* reference)
{
	const Component* component =
		ResolveComponent(reference, false);
	if (!component)
		return "<null Component>";
	return std::string(component->GetTypeStr()) + " (" +
		std::to_string(component->GetUID()) + ")";
}

bool RegisterConvertApi(asIScriptEngine& engine, std::string& error)
{
	engine.SetDefaultNamespace("Convert");
	const bool registered =
		engine.RegisterGlobalFunction("string ToString(int value)", asFUNCTIONPR(ConvertToString, (int), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(uint value)", asFUNCTIONPR(ConvertToString, (unsigned int), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(double value)", asFUNCTIONPR(ConvertToString, (double), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(float value)", asFUNCTIONPR(ConvertToString, (float), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(bool value)", asFUNCTIONPR(ConvertToString, (bool), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(const Vector2 &in value)", asFUNCTIONPR(ConvertToString, (const ScriptVector2&), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(const Vector3 &in value)", asFUNCTIONPR(ConvertToString, (const ScriptVector3&), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(const Quaternion &in value)", asFUNCTIONPR(ConvertToString, (const ScriptQuaternion&), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(const Color &in value)", asFUNCTIONPR(ConvertToString, (const ScriptColor&), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(GameObject@+ object)", asFUNCTIONPR(ConvertToString, (const ScriptGameObjectReference*), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(Transform@+ transform)", asFUNCTION(ConvertTransformToString), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(Component@+ component)", asFUNCTION(ConvertComponentToString), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseInt(const string &in text, int &out value)", asFUNCTION(TryParseInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseUInt(const string &in text, uint &out value)", asFUNCTION(TryParseUInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseDouble(const string &in text, double &out value)", asFUNCTION(TryParseDouble), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseFloat(const string &in text, float &out value)", asFUNCTION(TryParseFloat), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseBool(const string &in text, bool &out value)", asFUNCTION(TryParseBool), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseVector2(const string &in text, Vector2 &out value)", asFUNCTION(TryParseVector2), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseVector3(const string &in text, Vector3 &out value)", asFUNCTION(TryParseVector3), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseQuaternion(const string &in text, Quaternion &out value)", asFUNCTION(TryParseQuaternion), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseColor(const string &in text, Color &out value)", asFUNCTION(TryParseColor), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("int ToInt(const string &in text, int fallback = 0)", asFUNCTION(ConvertToInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("uint ToUInt(const string &in text, uint fallback = 0)", asFUNCTION(ConvertToUInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("double ToDouble(const string &in text, double fallback = 0)", asFUNCTION(ConvertToDouble), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("float ToFloat(const string &in text, float fallback = 0)", asFUNCTION(ConvertToFloat), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool ToBool(const string &in text, bool fallback = false)", asFUNCTION(ConvertToBool), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Vector2 ToVector2(const string &in text)", asFUNCTIONPR(ConvertToVector2, (const std::string&), ScriptVector2), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Vector2 ToVector2(const string &in text, const Vector2 &in fallback)", asFUNCTIONPR(ConvertToVector2, (const std::string&, const ScriptVector2&), ScriptVector2), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Vector3 ToVector3(const string &in text)", asFUNCTIONPR(ConvertToVector3, (const std::string&), ScriptVector3), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Vector3 ToVector3(const string &in text, const Vector3 &in fallback)", asFUNCTIONPR(ConvertToVector3, (const std::string&, const ScriptVector3&), ScriptVector3), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Quaternion ToQuaternion(const string &in text)", asFUNCTIONPR(ConvertToQuaternion, (const std::string&), ScriptQuaternion), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Quaternion ToQuaternion(const string &in text, const Quaternion &in fallback)", asFUNCTIONPR(ConvertToQuaternion, (const std::string&, const ScriptQuaternion&), ScriptQuaternion), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Color ToColor(const string &in text)", asFUNCTIONPR(ConvertToColor, (const std::string&), ScriptColor), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Color ToColor(const string &in text, const Color &in fallback)", asFUNCTIONPR(ConvertToColor, (const std::string&, const ScriptColor&), ScriptColor), asCALL_CDECL) >= 0;
	engine.SetDefaultNamespace("");
	if (registered)
		return true;

	error = "Could not register the Convert API.";
	return false;
}

std::string GetGameObjectName(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject ? gameObject->name : std::string();
}

void SetGameObjectName(
	const std::string& name,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->name = name;
}

bool GetGameObjectActive(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject && gameObject->IsActive();
}

void SetGameObjectActive(
	bool active,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetActive(active);
}

ScriptGameObjectReference* GetGameObjectTransform(
	ScriptGameObjectReference* reference)
{
	if (!ResolveGameObject(reference))
		return nullptr;
	reference->AddRef();
	return reference;
}

bool GameObjectsEqual(
	const ScriptGameObjectReference* other,
	const ScriptGameObjectReference* reference)
{
	return other == reference ||
		(other && reference &&
			other->GetObjectId() == reference->GetObjectId());
}

ScriptGameObjectReference* GetParent(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	const GameObject* parent =
		gameObject ? gameObject->GetParent() : nullptr;
	return parent && parent != App->level->GetRoot()
		? MakeGameObjectReference(parent->GetUID())
		: nullptr;
}

ScriptGameObjectReference* FindChild(
	const std::string& name,
	bool recursive,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	const GameObject* child =
		gameObject
			? gameObject->FindChild(name.c_str(), recursive)
			: nullptr;
	return child
		? MakeGameObjectReference(child->GetUID())
		: nullptr;
}

ScriptVector3 GetLocalPosition(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetLocalPosition())
		: ScriptVector3{};
}

void SetLocalPosition(
	const ScriptVector3& position,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetLocalPosition(ToEngineVector3(position));
}

ScriptVector3 GetLocalEulerAngles(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetLocalRotation())
		: ScriptVector3{};
}

void SetLocalEulerAngles(
	const ScriptVector3& eulerAngles,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetLocalRotation(ToEngineVector3(eulerAngles));
}

ScriptVector3 GetLocalEulerAnglesYXZ(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject)
		return {};

	const float3 orderedAngles =
		gameObject->GetLocalRotationQ().ToEulerYXZ();
	return {
		orderedAngles.y,
		orderedAngles.x,
		orderedAngles.z};
}

void SetLocalEulerAnglesYXZ(
	const ScriptVector3& eulerAngles,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
	{
		gameObject->SetLocalRotation(Quat::FromEulerYXZ(
			eulerAngles.y,
			eulerAngles.x,
			eulerAngles.z));
	}
}

ScriptVector3 GetLocalScale(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetLocalScale())
		: ScriptVector3{1.0f, 1.0f, 1.0f};
}

void SetLocalScale(
	const ScriptVector3& scale,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetLocalScale(ToEngineVector3(scale));
}

ScriptQuaternion GetLocalRotation(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptQuaternion(gameObject->GetLocalRotationQ())
		: ScriptQuaternion{};
}

void SetLocalRotation(
	const ScriptQuaternion& rotation,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetLocalRotation(ToEngineQuaternion(rotation));
}

ScriptQuaternion GetRotation(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptQuaternion(gameObject->GetGlobalRotationQ())
		: ScriptQuaternion{};
}

void SetRotation(
	const ScriptQuaternion& rotation,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetGlobalRotation(ToEngineQuaternion(rotation));
}

ScriptVector3 GetPosition(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetGlobalPosition())
		: ScriptVector3{};
}

void SetPosition(
	const ScriptVector3& position,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->SetGlobalPosition(ToEngineVector3(position));
}

ScriptVector3 GetEulerAngles(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetGlobalRotationQ().ToEulerXYZ())
		: ScriptVector3{};
}

void SetEulerAngles(
	const ScriptVector3& angles,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
	{
		gameObject->SetGlobalRotation(Quat::FromEulerXYZ(
			angles.x, angles.y, angles.z));
	}
}

ScriptVector3 GetLossyScale(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetGlobalScale())
		: ScriptVector3{1.0f, 1.0f, 1.0f};
}

void Translate(
	const ScriptVector3& translation,
	ScriptGameObjectReference* reference)
{
	if (GameObject* gameObject = ResolveGameObject(reference))
		gameObject->Move(ToEngineVector3(translation));
}

void TranslateInSpace(
	const ScriptVector3& translation,
	int space,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject)
		return;

	float3 delta = ToEngineVector3(translation);
	if (space == 1)
		delta = gameObject->GetGlobalRotationQ().Transform(delta);
	gameObject->SetGlobalPosition(gameObject->GetGlobalPosition() + delta);
}

void RotateTransform(
	const ScriptVector3& eulerAngles,
	int space,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject)
		return;

	const Quat delta = Quat::FromEulerXYZ(
		eulerAngles.x, eulerAngles.y, eulerAngles.z);
	if (space == 1)
		gameObject->SetLocalRotation(
			gameObject->GetLocalRotationQ() * delta);
	else
		gameObject->SetGlobalRotation(
			delta * gameObject->GetGlobalRotationQ());
}

void RotateAround(
	const ScriptVector3& point,
	const ScriptVector3& axis,
	float radians,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	const float3 nativeAxis = ToEngineVector3(axis);
	if (!gameObject || nativeAxis.LengthSq() <= 0.0000001f)
		return;

	const Quat delta = Quat::RotateAxisAngle(
		nativeAxis.Normalized(), radians);
	const float3 nativePoint = ToEngineVector3(point);
	gameObject->SetGlobalPosition(
		nativePoint +
			delta.Transform(gameObject->GetGlobalPosition() - nativePoint));
	gameObject->SetGlobalRotation(
		delta * gameObject->GetGlobalRotationQ());
}

void LookAt(
	const ScriptVector3& target,
	const ScriptVector3& worldUp,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject)
		return;
	const float3 direction =
		ToEngineVector3(target) - gameObject->GetGlobalPosition();
	const float3 up = ToEngineVector3(worldUp);
	if (direction.LengthSq() <= 0.0000001f ||
		up.LengthSq() <= 0.0000001f)
	{
		return;
	}
	gameObject->SetGlobalRotation(Quat::LookAt(
		-float3::unitZ,
		direction.Normalized(),
		float3::unitY,
		up.Normalized()));
}

ScriptVector3 TransformPoint(
	const ScriptVector3& point,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetCalculatedGlobalTransform().TransformPos(
				ToEngineVector3(point)))
		: ScriptVector3{};
}

ScriptVector3 InverseTransformPoint(
	const ScriptVector3& point,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetCalculatedGlobalTransform().Inverted().
				TransformPos(ToEngineVector3(point)))
		: ScriptVector3{};
}

ScriptVector3 TransformVector(
	const ScriptVector3& vector,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetCalculatedGlobalTransform().TransformDir(
				ToEngineVector3(vector)))
		: ScriptVector3{};
}

ScriptVector3 InverseTransformVector(
	const ScriptVector3& vector,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetCalculatedGlobalTransform().Inverted().
				TransformDir(ToEngineVector3(vector)))
		: ScriptVector3{};
}

ScriptVector3 TransformDirection(
	const ScriptVector3& direction,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetGlobalRotationQ().Transform(
			ToEngineVector3(direction)))
		: ScriptVector3{};
}

ScriptVector3 InverseTransformDirection(
	const ScriptVector3& direction,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(gameObject->GetGlobalRotationQ().Inverted().
			Transform(ToEngineVector3(direction)))
		: ScriptVector3{};
}

ScriptGameObjectReference* GetTransformParent(
	const ScriptGameObjectReference* reference)
{
	return GetParent(reference);
}

void SetTransformParent(
	ScriptGameObjectReference* parentReference,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	GameObject* newParent = ResolveGameObject(parentReference, false);
	if (!gameObject || !App || !App->level)
		return;
	gameObject->SetNewParent(
		newParent ? newParent : App->level->GetRoot(),
		true);
}

void SetParent(
	ScriptGameObjectReference* parentReference,
	bool worldPositionStays,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	GameObject* newParent = ResolveGameObject(parentReference, false);
	if (!gameObject || !App || !App->level)
		return;
	gameObject->SetNewParent(
		newParent ? newParent : App->level->GetRoot(),
		worldPositionStays);
}

unsigned int GetChildCount(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? static_cast<unsigned int>(gameObject->childs.size())
		: 0;
}

ScriptGameObjectReference* GetChild(
	unsigned int index,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject || index >= gameObject->childs.size())
		return nullptr;
	auto child = gameObject->childs.begin();
	std::advance(child, index);
	return *child
		? MakeGameObjectReference((*child)->GetUID())
		: nullptr;
}

bool IsChildOf(
	const ScriptGameObjectReference* parentReference,
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	const GameObject* possibleParent =
		ResolveGameObject(parentReference, false);
	return gameObject && possibleParent &&
		gameObject->IsUnder(possibleParent);
}

void SetPositionAndRotation(
	const ScriptVector3& position,
	const ScriptQuaternion& rotation,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject)
		return;
	gameObject->SetGlobalTransform(float4x4::FromTRS(
		ToEngineVector3(position),
		ToEngineQuaternion(rotation),
		gameObject->GetGlobalScale()));
}

void SetLocalPositionAndRotation(
	const ScriptVector3& position,
	const ScriptQuaternion& rotation,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject)
		return;
	gameObject->SetLocalPosition(ToEngineVector3(position));
	gameObject->SetLocalRotation(ToEngineQuaternion(rotation));
}

ScriptVector3 GetForward(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetGlobalRotationQ().Transform(
				-float3::unitZ).Normalized())
		: ScriptVector3{0.0f, 0.0f, -1.0f};
}

void SetForward(
	const ScriptVector3& direction,
	ScriptGameObjectReference* reference)
{
	GameObject* gameObject = ResolveGameObject(reference);
	const float3 forward = ToEngineVector3(direction);
	if (!gameObject || forward.LengthSq() <= 0.0000001f)
		return;
	float3 up = gameObject->GetGlobalRotationQ().
		Transform(float3::unitY).Normalized();
	if (std::abs(up.Dot(forward.Normalized())) > 0.999f)
		up = float3::unitY;
	gameObject->SetGlobalRotation(Quat::LookAt(
		-float3::unitZ,
		forward.Normalized(),
		float3::unitY,
		up));
}

ScriptGameObjectReference* GetTransformGameObject(
	ScriptGameObjectReference* reference)
{
	if (!ResolveGameObject(reference))
		return nullptr;
	reference->AddRef();
	return reference;
}

ScriptGameObjectReference* GetTransformRoot(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	if (!gameObject || !App || !App->level)
		return nullptr;
	const GameObject* root = gameObject;
	while (root->GetParent() &&
		root->GetParent() != App->level->GetRoot())
	{
		root = root->GetParent();
	}
	return MakeGameObjectReference(root->GetUID());
}

ScriptVector3 GetRight(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetGlobalRotationQ().Transform(
				float3::unitX).Normalized())
		: ScriptVector3{1.0f, 0.0f, 0.0f};
}

ScriptVector3 GetUp(
	const ScriptGameObjectReference* reference)
{
	const GameObject* gameObject = ResolveGameObject(reference);
	return gameObject
		? ToScriptVector3(
			gameObject->GetGlobalRotationQ().Transform(
				float3::unitY).Normalized())
		: ScriptVector3{0.0f, 1.0f, 0.0f};
}

ScriptGameObjectReference* FindGameObject(
	const std::string& name)
{
	const GameObject* gameObject =
		App && App->level ? App->level->Find(name.c_str()) : nullptr;
	return gameObject
		? MakeGameObjectReference(gameObject->GetUID())
		: nullptr;
}

ScriptGameObjectReference* FindGameObjectById(unsigned int id)
{
	const GameObject* gameObject =
		App && App->level ? App->level->Find(id) : nullptr;
	return gameObject
		? MakeGameObjectReference(gameObject->GetUID())
		: nullptr;
}

ScriptGameObjectReference* CreateGameObject(
	const std::string& name)
{
	GameObject* gameObject =
		App && App->level
			? App->level->CreateGameObject(name.c_str())
			: nullptr;
	return gameObject
		? MakeGameObjectReference(gameObject->GetUID())
		: nullptr;
}

void DestroyGameObject(ScriptGameObjectReference* reference)
{
	if (!App || !App->level)
		return;
	if (GameObject* gameObject =
		ResolveGameObject(reference, false))
	{
		App->level->DestroyGameObject(gameObject);
	}
}

std::string GetComponentTypeName(
	const ScriptComponentReference* reference)
{
	const Component* component = ResolveComponent(reference);
	return component ? component->GetTypeStr() : std::string();
}

ScriptGameObjectReference* GetComponentGameObject(
	const ScriptComponentReference* reference)
{
	const Component* component = ResolveComponent(reference);
	const GameObject* gameObject =
		component ? component->GetGameObject() : nullptr;
	return gameObject
		? MakeGameObjectReference(gameObject->GetUID())
		: nullptr;
}

bool ComponentsEqual(
	const ScriptComponentReference* other,
	const ScriptComponentReference* reference)
{
	return other == reference ||
		(other && reference &&
			other->GetObjectId() == reference->GetObjectId() &&
			other->GetComponentId() == reference->GetComponentId());
}

bool RegisterGameObjectApi(asIScriptEngine& engine, std::string& error)
{
	if (engine.RegisterEnum("Space") < 0 ||
		engine.RegisterEnumValue("Space", "World", 0) < 0 ||
		engine.RegisterEnumValue("Space", "Self", 1) < 0)
	{
		error = "Could not register the Transform Space enum.";
		return false;
	}

	const bool registered =
		engine.RegisterObjectMethod(
			"GameObject", "uint get_id() const property",
			asMETHOD(ScriptGameObjectReference, GetObjectId),
			asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "bool get_valid() const property",
			asMETHOD(ScriptGameObjectReference, IsValid),
			asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "string get_name() const property",
			asFUNCTION(GetGameObjectName), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject",
			"void set_name(const string &in) property",
			asFUNCTION(SetGameObjectName), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "bool get_active() const property",
			asFUNCTION(GetGameObjectActive),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "void set_active(bool) property",
			asFUNCTION(SetGameObjectActive),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "Transform@ get_transform() const property",
			asFUNCTION(GetGameObjectTransform), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject",
			"bool opEquals(const GameObject@+ other) const",
			asFUNCTION(GameObjectsEqual),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "GameObject@ get_parent() const property",
			asFUNCTION(GetParent), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject",
			"GameObject@ FindChild(const string &in name, "
				"bool recursive = true) const",
			asFUNCTION(FindChild), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 get_localPosition() const property",
			asFUNCTION(GetLocalPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_localPosition(const Vector3 &in) property",
			asFUNCTION(SetLocalPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 get_localEulerAngles() const property",
			asFUNCTION(GetLocalEulerAngles), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_localEulerAngles("
				"const Vector3 &in) property",
			asFUNCTION(SetLocalEulerAngles), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 get_localEulerAnglesYXZ() const property",
			asFUNCTION(GetLocalEulerAnglesYXZ),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_localEulerAnglesYXZ("
				"const Vector3 &in) property",
			asFUNCTION(SetLocalEulerAnglesYXZ),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Quaternion get_localRotation() const property",
			asFUNCTION(GetLocalRotation), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_localRotation(const Quaternion &in) property",
			asFUNCTION(SetLocalRotation), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 get_localScale() const property",
			asFUNCTION(GetLocalScale), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_localScale(const Vector3 &in) property",
			asFUNCTION(SetLocalScale), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_position() const property",
			asFUNCTION(GetPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_position(const Vector3 &in) property",
			asFUNCTION(SetPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Quaternion get_rotation() const property",
			asFUNCTION(GetRotation), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_rotation(const Quaternion &in) property",
			asFUNCTION(SetRotation), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_eulerAngles() const property",
			asFUNCTION(GetEulerAngles), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_eulerAngles(const Vector3 &in) property",
			asFUNCTION(SetEulerAngles), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_lossyScale() const property",
			asFUNCTION(GetLossyScale), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "void Translate(const Vector3 &in)",
			asFUNCTION(Translate), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void Translate(const Vector3 &in, Space)",
			asFUNCTION(TranslateInSpace), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void Rotate(const Vector3 &in radians, Space space = Space::Self)",
			asFUNCTION(RotateTransform), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void RotateAround(const Vector3 &in point, "
				"const Vector3 &in axis, float radians)",
			asFUNCTION(RotateAround), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void LookAt(const Vector3 &in target, "
				"const Vector3 &in worldUp = Math::Vector3Up)",
			asFUNCTION(LookAt), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 TransformPoint(const Vector3 &in) const",
			asFUNCTION(TransformPoint), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 InverseTransformPoint(const Vector3 &in) const",
			asFUNCTION(InverseTransformPoint), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 TransformVector(const Vector3 &in) const",
			asFUNCTION(TransformVector), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 InverseTransformVector(const Vector3 &in) const",
			asFUNCTION(InverseTransformVector), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 TransformDirection(const Vector3 &in) const",
			asFUNCTION(TransformDirection), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"Vector3 InverseTransformDirection(const Vector3 &in) const",
			asFUNCTION(InverseTransformDirection),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Transform@ get_parent() const property",
			asFUNCTION(GetTransformParent), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_parent(Transform@+ parent) property",
			asFUNCTION(SetTransformParent), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void SetParent(Transform@+ parent, "
				"bool worldPositionStays = true)",
			asFUNCTION(SetParent), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "uint get_childCount() const property",
			asFUNCTION(GetChildCount), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Transform@ GetChild(uint index) const",
			asFUNCTION(GetChild), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "bool IsChildOf(Transform@+ parent) const",
			asFUNCTION(IsChildOf), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void SetPositionAndRotation(const Vector3 &in position, "
				"const Quaternion &in rotation)",
			asFUNCTION(SetPositionAndRotation),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void SetLocalPositionAndRotation("
				"const Vector3 &in position, "
				"const Quaternion &in rotation)",
			asFUNCTION(SetLocalPositionAndRotation),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_forward() const property",
			asFUNCTION(GetForward), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"void set_forward(const Vector3 &in) property",
			asFUNCTION(SetForward), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_right() const property",
			asFUNCTION(GetRight), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_up() const property",
			asFUNCTION(GetUp), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform",
			"GameObject@ get_gameObject() const property",
			asFUNCTION(GetTransformGameObject),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Transform@ get_root() const property",
			asFUNCTION(GetTransformRoot), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "bool get_valid() const property",
			asMETHOD(ScriptGameObjectReference, IsValid),
			asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"Component", "uint get_id() const property",
			asMETHOD(ScriptComponentReference, GetComponentId),
			asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"Component", "bool get_valid() const property",
			asMETHOD(ScriptComponentReference, IsValid),
			asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"Component", "string get_typeName() const property",
			asFUNCTION(GetComponentTypeName),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Component",
			"GameObject@ get_gameObject() const property",
			asFUNCTION(GetComponentGameObject),
			asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Component",
			"bool opEquals(const Component@+ other) const",
			asFUNCTION(ComponentsEqual),
			asCALL_CDECL_OBJLAST) >= 0;
	if (!registered)
	{
		error =
			"Could not register the GameObject, Transform or Component API.";
		return false;
	}
	if (!RegisterTypedComponentApi(engine, error))
		return false;

	engine.SetDefaultNamespace("GameObject");
	const bool sceneApiRegistered =
		engine.RegisterGlobalFunction(
			"GameObject@ Find(const string &in name)",
			asFUNCTION(FindGameObject), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction(
			"GameObject@ FindById(uint id)",
			asFUNCTION(FindGameObjectById), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction(
			"GameObject@ FindByUUID(uint uuid)",
			asFUNCTION(FindGameObjectById), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction(
			"GameObject@ Create("
				"const string &in name = \"GameObject\")",
			asFUNCTION(CreateGameObject), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction(
			"void Destroy(GameObject@+ object)",
			asFUNCTION(DestroyGameObject), asCALL_CDECL) >= 0;
	engine.SetDefaultNamespace("");
	if (sceneApiRegistered)
		return true;

	error = "Could not register the GameObject scene API.";
	return false;
}

bool ScriptGetKey(int key)
{
    KeyState s = App->input->GetKey(key);
    return s == KEY_DOWN || s == KEY_REPEAT;
}

bool ScriptGetKeyDown(int key)
{
    return App->input->GetKey(key) == KEY_DOWN;
}

bool ScriptGetKeyUp(int key)
{
    return App->input->GetKey(key) == KEY_UP;
}

bool ScriptGetMouseButton(int button)
{
    if (button < 1 || button > NUM_MOUSE_BUTTONS)
        return false;
    KeyState s = App->input->GetMouseButton(button);
    return s == KEY_DOWN || s == KEY_REPEAT;
}

bool ScriptGetMouseButtonDown(int button)
{
    if (button < 1 || button > NUM_MOUSE_BUTTONS)
        return false;
    return App->input->GetMouseButton(button) == KEY_DOWN;
}

bool ScriptGetMouseButtonUp(int button)
{
    if (button < 1 || button > NUM_MOUSE_BUTTONS)
        return false;
    return App->input->GetMouseButton(button) == KEY_UP;
}

bool ScriptGetCursorLocked()
{
    return App && App->input && App->input->IsCursorLocked();
}

void ScriptSetCursorLocked(bool locked)
{
    if (App && App->input)
        App->input->SetCursorLocked(locked);
}

int ScriptGetMouseX()
{
    int x, y;
    App->input->GetMousePosition(x, y);
    return x;
}

int ScriptGetMouseY()
{
    int x, y;
    App->input->GetMousePosition(x, y);
    return y;
}

int ScriptGetMouseWheel()
{
    return App->input->GetMouseWheel();
}

float ScriptGetAxis(const std::string& axis)
{
    if (axis == "Horizontal")
    {
        if (ScriptGetKey(SDL_SCANCODE_A) || ScriptGetKey(SDL_SCANCODE_LEFT))  return -1.0f;
        if (ScriptGetKey(SDL_SCANCODE_D) || ScriptGetKey(SDL_SCANCODE_RIGHT)) return  1.0f;
        return 0.0f;
    }
    if (axis == "Vertical")
    {
        if (ScriptGetKey(SDL_SCANCODE_W) || ScriptGetKey(SDL_SCANCODE_UP))    return  1.0f;
        if (ScriptGetKey(SDL_SCANCODE_S) || ScriptGetKey(SDL_SCANCODE_DOWN))  return -1.0f;
        return 0.0f;
    }
    if (axis == "Mouse X")
    {
        int x, y;
        App->input->GetMouseMotion(x, y);
        return static_cast<float>(x);
    }
    if (axis == "Mouse Y")
    {
        int x, y;
        App->input->GetMouseMotion(x, y);
        return static_cast<float>(y);
    }
    return 0.0f;
}

} // névtelen namespace

// =============================================================================
// KeyCode enum regisztrálása
// =============================================================================
static bool ValidateFlyCameraApi(
	asIScriptEngine& engine,
	std::string& error)
{
	constexpr const char* ModuleName =
		"__EGE_FlyCameraApiValidation";
	constexpr const char* Source = R"(
void ValidateFlyCameraApi(Transform@ transform, float deltaTime)
{
    float lookSensitivity = 0.003f;
    Vector3 rotation = transform.localEulerAnglesYXZ;
    rotation.y -= Input::GetAxis("Mouse X") * lookSensitivity;
    rotation.x = Math::Clamp(
        rotation.x - Input::GetAxis("Mouse Y") * lookSensitivity,
        -89.0f * Math::Deg2Rad,
        89.0f * Math::Deg2Rad);
    rotation.z = 0.0f;
    transform.localEulerAnglesYXZ = rotation;

    Vector3 movement =
        transform.right * Input::GetAxis("Horizontal") +
        transform.forward * Input::GetAxis("Vertical");
    if (Input::GetKey(KeyCode::E))
        movement += transform.up;
    if (Input::GetKey(KeyCode::Q))
        movement -= transform.up;
    if (movement.lengthSquared > Math::Epsilon)
    {
        movement.Normalize();
        float speed = Input::GetKey(KeyCode::LeftShift)
            ? 18.0f
            : 6.0f;
        transform.Translate(movement * speed * deltaTime);
    }

    if (Input::GetKeyDown(KeyCode::Tab))
        Input::cursorLocked = !Input::cursorLocked;
    Input::SetCursorLocked(Input::cursorLocked);
}
)";

	asIScriptModule* module =
		engine.GetModule(ModuleName, asGM_ALWAYS_CREATE);
	if (!module ||
		module->AddScriptSection(
			"FlyCameraApiValidation", Source) < 0 ||
		module->Build() < 0)
	{
		engine.DiscardModule(ModuleName);
		error =
			"The Input and Transform fly-camera API did not "
			"pass AngelScript compile validation.";
		return false;
	}

	engine.DiscardModule(ModuleName);
	return true;
}

static bool ValidateExpandedEngineApi(
	asIScriptEngine& engine,
	std::string& error)
{
	constexpr const char* ModuleName =
		"__EGE_ExpandedEngineApiValidation";
	constexpr const char* Source = R"(
class ExpandedApiValidation
{
    Vector2 viewportOffset = Vector2(0.25f, 0.5f);
    Quaternion spawnRotation =
        Quaternion::Euler(Vector3(0.0f, 1.0f, 0.0f));
    Camera@ camera;

    void Exercise(GameObject@ owner, Transform@ transform)
    {
        Vector2 direction = Math::Vector2Right + Math::Vector2Up;
        direction.Normalize();
        Quaternion look = Quaternion::LookRotation(
            Vector3(0.0f, 0.0f, -1.0f));
        transform.localRotation = look;
        transform.position = transform.position + Vector3(1.0f, 0.0f, 0.0f);
        transform.rotation = Quaternion::Slerp(
            transform.rotation, spawnRotation, 0.5f);
        transform.Rotate(Vector3(0.0f, 0.1f, 0.0f), Space::Self);
        transform.LookAt(Vector3(0.0f, 0.0f, 0.0f));
        Vector3 world = transform.TransformPoint(Vector3(1.0f, 0.0f, 0.0f));
        Vector3 local = transform.InverseTransformPoint(world);
        transform.SetPositionAndRotation(world, look);
        uint children = transform.childCount;
        if (children > 0)
            transform.GetChild(0).SetParent(transform, true);

        Camera@ typedCamera = owner.GetComponent<Camera>();
        if (typedCamera !is null)
        {
            typedCamera.fieldOfView = 70.0f;
            Vector3 screen = typedCamera.WorldToScreenPoint(world);
            typedCamera.ScreenToWorldPoint(screen);
            typedCamera.ScreenPointToDirection(viewportOffset);
        }
        Camera@ mainCamera = Camera::main;

        bool pending = Scene::loadPending;
        string sceneName = Scene::name;
        Prefab::Instantiate(
            "Assets/Test.egeprefab", world, look, owner);
        Scene::Load("Assets/Test.eduscene");
        Scene::Reload();
    }
}
)";

	asIScriptModule* module =
		engine.GetModule(ModuleName, asGM_ALWAYS_CREATE);
	if (!module ||
		module->AddScriptSection("ExpandedEngineApiValidation", Source) < 0 ||
		module->Build() < 0)
	{
		engine.DiscardModule(ModuleName);
		error =
			"The Vector2, Quaternion, Transform, Camera, Scene and Prefab "
			"APIs did not pass AngelScript compile validation.";
		return false;
	}
	engine.DiscardModule(ModuleName);
	return true;
}

static bool RegisterKeyCodeEnum(
    asIScriptEngine* engine,
    std::string& error)
{
    if (engine->RegisterEnum("KeyCode") < 0)
    {
        error = "Could not register KeyCode.";
        return false;
    }

    // Betűk
    engine->RegisterEnumValue("KeyCode", "A", SDL_SCANCODE_A);
    engine->RegisterEnumValue("KeyCode", "B", SDL_SCANCODE_B);
    engine->RegisterEnumValue("KeyCode", "C", SDL_SCANCODE_C);
    engine->RegisterEnumValue("KeyCode", "D", SDL_SCANCODE_D);
    engine->RegisterEnumValue("KeyCode", "E", SDL_SCANCODE_E);
    engine->RegisterEnumValue("KeyCode", "F", SDL_SCANCODE_F);
    engine->RegisterEnumValue("KeyCode", "G", SDL_SCANCODE_G);
    engine->RegisterEnumValue("KeyCode", "H", SDL_SCANCODE_H);
    engine->RegisterEnumValue("KeyCode", "I", SDL_SCANCODE_I);
    engine->RegisterEnumValue("KeyCode", "J", SDL_SCANCODE_J);
    engine->RegisterEnumValue("KeyCode", "K", SDL_SCANCODE_K);
    engine->RegisterEnumValue("KeyCode", "L", SDL_SCANCODE_L);
    engine->RegisterEnumValue("KeyCode", "M", SDL_SCANCODE_M);
    engine->RegisterEnumValue("KeyCode", "N", SDL_SCANCODE_N);
    engine->RegisterEnumValue("KeyCode", "O", SDL_SCANCODE_O);
    engine->RegisterEnumValue("KeyCode", "P", SDL_SCANCODE_P);
    engine->RegisterEnumValue("KeyCode", "Q", SDL_SCANCODE_Q);
    engine->RegisterEnumValue("KeyCode", "R", SDL_SCANCODE_R);
    engine->RegisterEnumValue("KeyCode", "S", SDL_SCANCODE_S);
    engine->RegisterEnumValue("KeyCode", "T", SDL_SCANCODE_T);
    engine->RegisterEnumValue("KeyCode", "U", SDL_SCANCODE_U);
    engine->RegisterEnumValue("KeyCode", "V", SDL_SCANCODE_V);
    engine->RegisterEnumValue("KeyCode", "W", SDL_SCANCODE_W);
    engine->RegisterEnumValue("KeyCode", "X", SDL_SCANCODE_X);
    engine->RegisterEnumValue("KeyCode", "Y", SDL_SCANCODE_Y);
    engine->RegisterEnumValue("KeyCode", "Z", SDL_SCANCODE_Z);

    // Számok
    engine->RegisterEnumValue("KeyCode", "Alpha0", SDL_SCANCODE_0);
    engine->RegisterEnumValue("KeyCode", "Alpha1", SDL_SCANCODE_1);
    engine->RegisterEnumValue("KeyCode", "Alpha2", SDL_SCANCODE_2);
    engine->RegisterEnumValue("KeyCode", "Alpha3", SDL_SCANCODE_3);
    engine->RegisterEnumValue("KeyCode", "Alpha4", SDL_SCANCODE_4);
    engine->RegisterEnumValue("KeyCode", "Alpha5", SDL_SCANCODE_5);
    engine->RegisterEnumValue("KeyCode", "Alpha6", SDL_SCANCODE_6);
    engine->RegisterEnumValue("KeyCode", "Alpha7", SDL_SCANCODE_7);
    engine->RegisterEnumValue("KeyCode", "Alpha8", SDL_SCANCODE_8);
    engine->RegisterEnumValue("KeyCode", "Alpha9", SDL_SCANCODE_9);

    // Funkcióbillentyűk
    engine->RegisterEnumValue("KeyCode", "F1", SDL_SCANCODE_F1);
    engine->RegisterEnumValue("KeyCode", "F2", SDL_SCANCODE_F2);
    engine->RegisterEnumValue("KeyCode", "F3", SDL_SCANCODE_F3);
    engine->RegisterEnumValue("KeyCode", "F4", SDL_SCANCODE_F4);
    engine->RegisterEnumValue("KeyCode", "F5", SDL_SCANCODE_F5);
    engine->RegisterEnumValue("KeyCode", "F6", SDL_SCANCODE_F6);
    engine->RegisterEnumValue("KeyCode", "F7", SDL_SCANCODE_F7);
    engine->RegisterEnumValue("KeyCode", "F8", SDL_SCANCODE_F8);
    engine->RegisterEnumValue("KeyCode", "F9", SDL_SCANCODE_F9);
    engine->RegisterEnumValue("KeyCode", "F10", SDL_SCANCODE_F10);
    engine->RegisterEnumValue("KeyCode", "F11", SDL_SCANCODE_F11);
    engine->RegisterEnumValue("KeyCode", "F12", SDL_SCANCODE_F12);

    // Navigáció
    engine->RegisterEnumValue("KeyCode", "UpArrow", SDL_SCANCODE_UP);
    engine->RegisterEnumValue("KeyCode", "DownArrow", SDL_SCANCODE_DOWN);
    engine->RegisterEnumValue("KeyCode", "LeftArrow", SDL_SCANCODE_LEFT);
    engine->RegisterEnumValue("KeyCode", "RightArrow", SDL_SCANCODE_RIGHT);

    // Speciális
    engine->RegisterEnumValue("KeyCode", "Space", SDL_SCANCODE_SPACE);
    engine->RegisterEnumValue("KeyCode", "Return", SDL_SCANCODE_RETURN);
    engine->RegisterEnumValue("KeyCode", "Escape", SDL_SCANCODE_ESCAPE);
    engine->RegisterEnumValue("KeyCode", "Tab", SDL_SCANCODE_TAB);
    engine->RegisterEnumValue("KeyCode", "Backspace", SDL_SCANCODE_BACKSPACE);
    engine->RegisterEnumValue("KeyCode", "Delete", SDL_SCANCODE_DELETE);
    engine->RegisterEnumValue("KeyCode", "Insert", SDL_SCANCODE_INSERT);
    engine->RegisterEnumValue("KeyCode", "Home", SDL_SCANCODE_HOME);
    engine->RegisterEnumValue("KeyCode", "End", SDL_SCANCODE_END);
    engine->RegisterEnumValue("KeyCode", "PageUp", SDL_SCANCODE_PAGEUP);
    engine->RegisterEnumValue("KeyCode", "PageDown", SDL_SCANCODE_PAGEDOWN);
    engine->RegisterEnumValue("KeyCode", "CapsLock", SDL_SCANCODE_CAPSLOCK);

    // Módosítók
    engine->RegisterEnumValue("KeyCode", "LeftShift", SDL_SCANCODE_LSHIFT);
    engine->RegisterEnumValue("KeyCode", "RightShift", SDL_SCANCODE_RSHIFT);
    engine->RegisterEnumValue("KeyCode", "LeftCtrl", SDL_SCANCODE_LCTRL);
    engine->RegisterEnumValue("KeyCode", "RightCtrl", SDL_SCANCODE_RCTRL);
    engine->RegisterEnumValue("KeyCode", "LeftAlt", SDL_SCANCODE_LALT);
    engine->RegisterEnumValue("KeyCode", "RightAlt", SDL_SCANCODE_RALT);

    // Numpad
    engine->RegisterEnumValue("KeyCode", "Keypad0", SDL_SCANCODE_KP_0);
    engine->RegisterEnumValue("KeyCode", "Keypad1", SDL_SCANCODE_KP_1);
    engine->RegisterEnumValue("KeyCode", "Keypad2", SDL_SCANCODE_KP_2);
    engine->RegisterEnumValue("KeyCode", "Keypad3", SDL_SCANCODE_KP_3);
    engine->RegisterEnumValue("KeyCode", "Keypad4", SDL_SCANCODE_KP_4);
    engine->RegisterEnumValue("KeyCode", "Keypad5", SDL_SCANCODE_KP_5);
    engine->RegisterEnumValue("KeyCode", "Keypad6", SDL_SCANCODE_KP_6);
    engine->RegisterEnumValue("KeyCode", "Keypad7", SDL_SCANCODE_KP_7);
    engine->RegisterEnumValue("KeyCode", "Keypad8", SDL_SCANCODE_KP_8);
    engine->RegisterEnumValue("KeyCode", "Keypad9", SDL_SCANCODE_KP_9);
    engine->RegisterEnumValue("KeyCode", "KeypadEnter", SDL_SCANCODE_KP_ENTER);
    engine->RegisterEnumValue("KeyCode", "KeypadPlus", SDL_SCANCODE_KP_PLUS);
    engine->RegisterEnumValue("KeyCode", "KeypadMinus", SDL_SCANCODE_KP_MINUS);
    engine->RegisterEnumValue("KeyCode", "KeypadMultiply", SDL_SCANCODE_KP_MULTIPLY);
    engine->RegisterEnumValue("KeyCode", "KeypadDivide", SDL_SCANCODE_KP_DIVIDE);
    engine->RegisterEnumValue("KeyCode", "KeypadPeriod", SDL_SCANCODE_KP_PERIOD);
    return true;
}

// =============================================================================
// Regisztráció
// =============================================================================
bool RegisterEngineBindings(
    asIScriptEngine& engine,
    std::string& error)
{
    // --- KeyCode enum ---
    error.clear();
	if (!RegisterMathApi(engine, error))
		return false;
	if (!RegisterDebugDrawApi(engine, error))
		return false;
	if (!RegisterCoreHelpersApi(engine, error))
		return false;
	if (!RegisterGameObjectApi(engine, error))
		return false;
	if (!RegisterSceneApi(engine, error))
		return false;
	if (!RegisterPhysicsApi(engine, error))
		return false;
	if (!RegisterPhysicsQueryApi(engine, error))
		return false;
	if (!RegisterConvertApi(engine, error))
		return false;
    if (!RegisterKeyCodeEnum(&engine, error))
        return false;

    // --- Input függvények ---
    engine.SetDefaultNamespace("Input");
    const bool registered =
        engine.RegisterGlobalFunction("bool GetKey(KeyCode key)", asFUNCTION(ScriptGetKey), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("bool GetKeyDown(KeyCode key)", asFUNCTION(ScriptGetKeyDown), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("bool GetKeyUp(KeyCode key)", asFUNCTION(ScriptGetKeyUp), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("bool GetMouseButton(int button)", asFUNCTION(ScriptGetMouseButton), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("bool GetMouseButtonDown(int button)", asFUNCTION(ScriptGetMouseButtonDown), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("bool GetMouseButtonUp(int button)", asFUNCTION(ScriptGetMouseButtonUp), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("int GetMouseX()", asFUNCTION(ScriptGetMouseX), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("int GetMouseY()", asFUNCTION(ScriptGetMouseY), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("int GetMouseWheel()", asFUNCTION(ScriptGetMouseWheel), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("float GetAxis(const string &in axis)", asFUNCTION(ScriptGetAxis), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("bool get_cursorLocked() property", asFUNCTION(ScriptGetCursorLocked), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("void set_cursorLocked(bool) property", asFUNCTION(ScriptSetCursorLocked), asCALL_CDECL) >= 0 &&
        engine.RegisterGlobalFunction("void SetCursorLocked(bool locked)", asFUNCTION(ScriptSetCursorLocked), asCALL_CDECL) >= 0;
    engine.SetDefaultNamespace("");
    if (!registered)
    {
        error = "Could not register the Input API.";
        return false;
    }
    return ValidateFlyCameraApi(engine, error) &&
		ValidateExpandedEngineApi(engine, error);
}

bool RunEngineBindingsSelfTest()
{
	asIScriptEngine* engine = asCreateScriptEngine();
	if (!engine)
		return false;
	engine->SetMessageCallback(
		asFUNCTION(SelfTestMessageCallback),
		nullptr,
		asCALL_CDECL);

	std::string error;
	RegisterStdString(engine);
	RegisterScriptArray(engine, true);
	bool passed =
		engine->RegisterObjectType(
			"GameObject", 0, asOBJ_REF) >= 0 &&
		engine->RegisterObjectType(
			"Transform", 0, asOBJ_REF) >= 0 &&
		engine->RegisterObjectType(
			"Component", 0, asOBJ_REF) >= 0 &&
		engine->RegisterObjectBehaviour(
			"GameObject", asBEHAVE_ADDREF, "void f()",
			asMETHOD(ScriptGameObjectReference, AddRef),
			asCALL_THISCALL) >= 0 &&
		engine->RegisterObjectBehaviour(
			"GameObject", asBEHAVE_RELEASE, "void f()",
			asMETHOD(ScriptGameObjectReference, Release),
			asCALL_THISCALL) >= 0 &&
		engine->RegisterObjectBehaviour(
			"Transform", asBEHAVE_ADDREF, "void f()",
			asMETHOD(ScriptGameObjectReference, AddRef),
			asCALL_THISCALL) >= 0 &&
		engine->RegisterObjectBehaviour(
			"Transform", asBEHAVE_RELEASE, "void f()",
			asMETHOD(ScriptGameObjectReference, Release),
			asCALL_THISCALL) >= 0 &&
		engine->RegisterObjectBehaviour(
			"Component", asBEHAVE_ADDREF, "void f()",
			asMETHOD(ScriptComponentReference, AddRef),
			asCALL_THISCALL) >= 0 &&
		engine->RegisterObjectBehaviour(
			"Component", asBEHAVE_RELEASE, "void f()",
			asMETHOD(ScriptComponentReference, Release),
			asCALL_THISCALL) >= 0;
	if (passed)
		passed = RegisterEngineBindings(*engine, error);
	if (passed)
	{
		constexpr const char* source = R"(
shared class EGEBehaviour
{
    GameObject@ gameObject;
    Transform@ transform;
    bool enabled = true;
}

class CompoundCollisionProbe : EGEBehaviour
{
    void OnCollisionEnter(CollisionInfo@ info)
    {
        Collider@ self = cast<Collider>(info.selfCollider);
        Collider@ other = cast<Collider>(info.collider);
        uint selfId = info.selfColliderId;
        uint otherId = info.otherColliderId;
        bool trigger = info.isTrigger;
        if (self !is null)
            self.enabled = self.enabled;
        if (other !is null)
            other.isTrigger = other.isTrigger;
    }

    void OnCollisionStay(CollisionInfo@ info) {}
    void OnCollisionExit(CollisionInfo@ info) {}
    void OnTriggerEnter(CollisionInfo@ info) {}
    void OnTriggerStay(CollisionInfo@ info) {}
    void OnTriggerExit(CollisionInfo@ info) {}

    void BuildCompound()
    {
        Collider@ first = gameObject.AddComponent<Collider>();
        Collider@ second = gameObject.AddComponent<Collider>();
        first.shape = ColliderShape::Box;
        second.shape = ColliderShape::Sphere;
        second.isTrigger = true;
    }

    void DrawDebugApi()
    {
        Debug::enabled = true;
        Debug::DrawPoint(Vector3(0, 0, 0));
        Debug::DrawPoint(Vector3(0, 0, 0), Math::ColorYellow, 4, 0.1f, false);
        Debug::DrawLine(Math::Vector3Zero, Math::Vector3One);
        Debug::DrawLine(Math::Vector3Zero, Math::Vector3One, Math::ColorGreen);
        Debug::DrawRay(Math::Vector3Zero, Math::Vector3Forward, Math::ColorCyan);
        Debug::DrawArrow(Math::Vector3Zero, Math::Vector3Up, Math::ColorRed);
        Debug::DrawCross(Math::Vector3Zero, Math::ColorWhite);
        Debug::DrawCircle(
            Math::Vector3Zero, Math::Vector3Up, Math::ColorBlue, 2);
        Debug::DrawSphere(Math::Vector3Zero, Math::ColorYellow, 1);
        Debug::DrawCapsule(
            Math::Vector3Zero, Math::Vector3Up, Math::ColorMagenta, 0.5f, 2);
        Debug::DrawCone(
            Math::Vector3Zero, Math::Vector3Up, Math::ColorWhite, 1);
        Debug::DrawBox(Math::Vector3Zero, Math::Vector3One, Math::ColorGreen);
        Debug::DrawBounds(
            Vector3(-1, -1, -1), Math::Vector3One, Math::ColorYellow);
        Debug::DrawPlane(
            Math::Vector3Zero, Math::Vector3Up, Math::ColorWhite, 4);
        Debug::DrawAxes(Math::Vector3Zero);
        Debug::DrawScreenText(
            "Debug API", Vector3(16, 16, 0), Math::ColorWhite);
    }

    void QueryPhysics()
    {
        RaycastHit@ hit;
        bool rayHit = Physics::Raycast(
            Math::Vector3Zero,
            Math::Vector3Forward,
            hit,
            100.0f,
            uint(1) << 3,
            false);
        array<RaycastHit@>@ rayHits = Physics::RaycastAll(
            Math::Vector3Zero,
            Math::Vector3Forward);
        bool sphereHit = Physics::SphereCast(
            Math::Vector3Zero,
            0.5f,
            Math::Vector3Forward,
            hit);
        array<RaycastHit@>@ overlaps = Physics::OverlapSphere(
            Math::Vector3Zero,
            2.0f);
        if (rayHit && sphereHit && hit !is null)
        {
            GameObject@ object = hit.gameObject;
            RigidBody@ body = hit.rigidBody;
            Collider@ collider = hit.collider;
            Vector3 point = hit.point;
            Vector3 normal = hit.normal;
            float distance = hit.distance;
            float fraction = hit.fraction;
            uint layer = hit.layer;
            bool trigger = hit.isTrigger;
        }
    }
}

class ScriptPeer : EGEBehaviour
{
    int value = 42;
}

class ScriptConsumer : EGEBehaviour
{
    void ResolvePeer()
    {
        ScriptPeer@ first = gameObject.GetComponent<ScriptPeer>();
        bool hasPeer = gameObject.HasComponent<ScriptPeer>();
        ScriptPeer@ tried;
        bool foundPeer =
            gameObject.TryGetComponent<ScriptPeer>(tried);
        array<ScriptPeer@>@ peers =
            gameObject.GetComponents<ScriptPeer@>();
    }
}
)";
		asIScriptModule* module = engine->GetModule(
			"EnginePhysicsApi", asGM_ALWAYS_CREATE);
		passed =
			module &&
			module->AddScriptSection(
				"EnginePhysicsApi.as", source) >= 0 &&
			module->Build() >= 0;
	}
	engine->ShutDownAndRelease();
	return passed;
}

}
