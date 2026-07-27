#include "ScriptBindings.h"

#include "../Application.h"
#include "../ModuleInput.h"
#include "../Globals.h"

#include <angelscript.h>
#include <SDL_scancode.h>

// =============================================================================
// Input wrapper függvények
// =============================================================================
namespace {

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
static void RegisterKeyCodeEnum(asIScriptEngine* engine)
{
    engine->RegisterEnum("KeyCode");

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
}

// =============================================================================
// Regisztráció
// =============================================================================
void RegisterEngineBindings(asIScriptEngine* engine)
{
    // --- KeyCode enum ---
    RegisterKeyCodeEnum(engine);

    // --- Input függvények ---
    engine->SetDefaultNamespace("Input");
    engine->RegisterGlobalFunction("bool GetKey(KeyCode key)", asFUNCTION(ScriptGetKey), asCALL_CDECL);
    engine->RegisterGlobalFunction("bool GetKeyDown(KeyCode key)", asFUNCTION(ScriptGetKeyDown), asCALL_CDECL);
    engine->RegisterGlobalFunction("bool GetKeyUp(KeyCode key)", asFUNCTION(ScriptGetKeyUp), asCALL_CDECL);
    engine->RegisterGlobalFunction("bool GetMouseButton(int button)", asFUNCTION(ScriptGetMouseButton), asCALL_CDECL);
    engine->RegisterGlobalFunction("bool GetMouseButtonDown(int button)", asFUNCTION(ScriptGetMouseButtonDown), asCALL_CDECL);
    engine->RegisterGlobalFunction("bool GetMouseButtonUp(int button)", asFUNCTION(ScriptGetMouseButtonUp), asCALL_CDECL);
    engine->RegisterGlobalFunction("int GetMouseX()", asFUNCTION(ScriptGetMouseX), asCALL_CDECL);
    engine->RegisterGlobalFunction("int GetMouseY()", asFUNCTION(ScriptGetMouseY), asCALL_CDECL);
    engine->RegisterGlobalFunction("int GetMouseWheel()", asFUNCTION(ScriptGetMouseWheel), asCALL_CDECL);
    engine->RegisterGlobalFunction("float GetAxis(const string &in axis)", asFUNCTION(ScriptGetAxis), asCALL_CDECL);
    engine->SetDefaultNamespace("");
}
