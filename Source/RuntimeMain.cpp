#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "EngineAPI.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return RunEngine(EngineMode::Runtime);
}
