#include "ScriptBindings.h"

#include "../Application.h"
#include "../GameObject.h"
#include "../Math.h"
#include "../ModuleInput.h"
#include "../Globals.h"

#include <angelscript.h>
#include <SDL_scancode.h>

#include <charconv>
#include <cctype>
#include <iomanip>
#include <new>
#include <sstream>
#include <string>
#include <string_view>

// =============================================================================
// Input wrapper függvények
// =============================================================================
namespace EGE
{
namespace {

struct ScriptVector3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

void ConstructVector3(
	float x,
	float y,
	float z,
	ScriptVector3* value)
{
	new (value) ScriptVector3{x, y, z};
}

ScriptVector3 ToScriptVector3(const float3& value)
{
	return {value.x, value.y, value.z};
}

float3 ToEngineVector3(const ScriptVector3& value)
{
	return {value.x, value.y, value.z};
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

std::string ConvertToString(const GameObject* gameObject)
{
	if (!gameObject)
		return "<null GameObject>";
	return gameObject->name + " (" + std::to_string(gameObject->GetUID()) + ")";
}

std::string ConvertTransformToString(const GameObject* transform)
{
	if (!transform)
		return "<null Transform>";
	return ConvertToString(
		ToScriptVector3(transform->GetGlobalPosition()));
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
		engine.RegisterGlobalFunction("string ToString(const Vector3 &in value)", asFUNCTIONPR(ConvertToString, (const ScriptVector3&), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(GameObject@ object)", asFUNCTIONPR(ConvertToString, (const GameObject*), std::string), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("string ToString(Transform@ transform)", asFUNCTION(ConvertTransformToString), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseInt(const string &in text, int &out value)", asFUNCTION(TryParseInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseUInt(const string &in text, uint &out value)", asFUNCTION(TryParseUInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseDouble(const string &in text, double &out value)", asFUNCTION(TryParseDouble), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseFloat(const string &in text, float &out value)", asFUNCTION(TryParseFloat), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseBool(const string &in text, bool &out value)", asFUNCTION(TryParseBool), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool TryParseVector3(const string &in text, Vector3 &out value)", asFUNCTION(TryParseVector3), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("int ToInt(const string &in text, int fallback = 0)", asFUNCTION(ConvertToInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("uint ToUInt(const string &in text, uint fallback = 0)", asFUNCTION(ConvertToUInt), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("double ToDouble(const string &in text, double fallback = 0)", asFUNCTION(ConvertToDouble), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("float ToFloat(const string &in text, float fallback = 0)", asFUNCTION(ConvertToFloat), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("bool ToBool(const string &in text, bool fallback = false)", asFUNCTION(ConvertToBool), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Vector3 ToVector3(const string &in text)", asFUNCTIONPR(ConvertToVector3, (const std::string&), ScriptVector3), asCALL_CDECL) >= 0 &&
		engine.RegisterGlobalFunction("Vector3 ToVector3(const string &in text, const Vector3 &in fallback)", asFUNCTIONPR(ConvertToVector3, (const std::string&, const ScriptVector3&), ScriptVector3), asCALL_CDECL) >= 0;
	engine.SetDefaultNamespace("");
	if (registered)
		return true;

	error = "Could not register the Convert API.";
	return false;
}

std::string GetGameObjectName(const GameObject* gameObject)
{
	return gameObject ? gameObject->name : std::string();
}

void SetGameObjectName(
	const std::string& name,
	GameObject* gameObject)
{
	if (gameObject)
		gameObject->name = name;
}

GameObject* GetGameObjectTransform(GameObject* gameObject)
{
	return gameObject;
}

ScriptVector3 GetLocalPosition(const GameObject* gameObject)
{
	return gameObject
		? ToScriptVector3(gameObject->GetLocalPosition())
		: ScriptVector3{};
}

void SetLocalPosition(
	const ScriptVector3& position,
	GameObject* gameObject)
{
	if (gameObject)
		gameObject->SetLocalPosition(ToEngineVector3(position));
}

ScriptVector3 GetLocalEulerAngles(const GameObject* gameObject)
{
	return gameObject
		? ToScriptVector3(gameObject->GetLocalRotation())
		: ScriptVector3{};
}

void SetLocalEulerAngles(
	const ScriptVector3& eulerAngles,
	GameObject* gameObject)
{
	if (gameObject)
		gameObject->SetLocalRotation(ToEngineVector3(eulerAngles));
}

ScriptVector3 GetLocalScale(const GameObject* gameObject)
{
	return gameObject
		? ToScriptVector3(gameObject->GetLocalScale())
		: ScriptVector3{1.0f, 1.0f, 1.0f};
}

void SetLocalScale(
	const ScriptVector3& scale,
	GameObject* gameObject)
{
	if (gameObject)
		gameObject->SetLocalScale(ToEngineVector3(scale));
}

ScriptVector3 GetPosition(const GameObject* gameObject)
{
	return gameObject
		? ToScriptVector3(gameObject->GetGlobalPosition())
		: ScriptVector3{};
}

void Translate(
	const ScriptVector3& translation,
	GameObject* gameObject)
{
	if (gameObject)
		gameObject->Move(ToEngineVector3(translation));
}

bool RegisterGameObjectApi(asIScriptEngine& engine, std::string& error)
{
	if (engine.RegisterObjectType(
			"Vector3", sizeof(ScriptVector3),
			asOBJ_VALUE | asOBJ_APP_CLASS_CDAK) < 0 ||
		engine.RegisterObjectBehaviour(
			"Vector3", asBEHAVE_CONSTRUCT,
			"void f(float x = 0, float y = 0, float z = 0)",
			asFUNCTION(ConstructVector3), asCALL_CDECL_OBJLAST) < 0 ||
		engine.RegisterObjectProperty(
			"Vector3", "float x", asOFFSET(ScriptVector3, x)) < 0 ||
		engine.RegisterObjectProperty(
			"Vector3", "float y", asOFFSET(ScriptVector3, y)) < 0 ||
		engine.RegisterObjectProperty(
			"Vector3", "float z", asOFFSET(ScriptVector3, z)) < 0)
	{
		error = "Could not register the Vector3 value type.";
		return false;
	}

	const bool registered =
		engine.RegisterObjectMethod(
			"GameObject", "uint get_id() const",
			asMETHOD(GameObject, GetUID), asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "string get_name() const",
			asFUNCTION(GetGameObjectName), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "void set_name(const string &in)",
			asFUNCTION(SetGameObjectName), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "bool get_active() const",
			asMETHOD(GameObject, IsActive), asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "void set_active(bool)",
			asMETHOD(GameObject, SetActive), asCALL_THISCALL) >= 0 &&
		engine.RegisterObjectMethod(
			"GameObject", "Transform@ get_transform() const",
			asFUNCTION(GetGameObjectTransform), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_localPosition() const",
			asFUNCTION(GetLocalPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "void set_localPosition(const Vector3 &in)",
			asFUNCTION(SetLocalPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_localEulerAngles() const",
			asFUNCTION(GetLocalEulerAngles), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "void set_localEulerAngles(const Vector3 &in)",
			asFUNCTION(SetLocalEulerAngles), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_localScale() const",
			asFUNCTION(GetLocalScale), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "void set_localScale(const Vector3 &in)",
			asFUNCTION(SetLocalScale), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "Vector3 get_position() const",
			asFUNCTION(GetPosition), asCALL_CDECL_OBJLAST) >= 0 &&
		engine.RegisterObjectMethod(
			"Transform", "void Translate(const Vector3 &in)",
			asFUNCTION(Translate), asCALL_CDECL_OBJLAST) >= 0;
	if (registered)
		return true;

	error = "Could not register the GameObject or Transform API.";
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
    KeyState s = App->input->GetMouseButton(button);
    return s == KEY_DOWN || s == KEY_REPEAT;
}

bool ScriptGetMouseButtonDown(int button)
{
    return App->input->GetMouseButton(button) == KEY_DOWN;
}

bool ScriptGetMouseButtonUp(int button)
{
    return App->input->GetMouseButton(button) == KEY_UP;
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
	if (!RegisterGameObjectApi(engine, error))
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
        engine.RegisterGlobalFunction("float GetAxis(const string &in axis)", asFUNCTION(ScriptGetAxis), asCALL_CDECL) >= 0;
    engine.SetDefaultNamespace("");
    if (registered)
        return true;

    error = "Could not register the Input API.";
    return false;
}

}
