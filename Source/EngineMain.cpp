#include "Globals.h"
#include "Application.h"
#include "EngineAPI.h"

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "Leaks.h"

#include <cstdlib>

namespace
{
    enum class MainState
    {
        Creation,
        Start,
        Update,
        Finish,
        Exit
    };

    void DumpLeaks()
    {
        _CrtDumpMemoryLeaks();
    }

    int __cdecl CrtDbgHook(int, char*, int*)
    {
        return 0;
    }
}

Application* App = nullptr;

int RunEngine(EngineMode mode)
{
    SDL_SetMainReady();
    _CrtSetReportHook2(_CRT_RPTHOOK_INSTALL, CrtDbgHook);

    LOG("Starting EDU Engine in %s mode",
        mode == EngineMode::Editor ? "Editor" : "Runtime");

#ifdef _DEBUG
    atexit(DumpLeaks);
#endif

    int mainReturn = EXIT_FAILURE;
    MainState state = MainState::Creation;

    while (state != MainState::Exit)
    {
        switch (state)
        {
        case MainState::Creation:
            LOG("-------------- Application Creation --------------");
            App = new Application(mode);
            state = MainState::Start;
            break;

        case MainState::Start:
            LOG("-------------- Application Init --------------");
            if (!App->Init())
            {
                LOG("Application Init exits with ERROR");
                state = MainState::Exit;
            }
            else
            {
                state = MainState::Update;
                LOG("-------------- Application Update --------------");
            }
            break;

        case MainState::Update:
        {
            const int updateReturn = App->Update();
            if (updateReturn == UPDATE_ERROR)
            {
                LOG("Application Update exits with ERROR");
                state = MainState::Exit;
            }
            else if (updateReturn == UPDATE_STOP)
            {
                state = MainState::Finish;
            }
            break;
        }

        case MainState::Finish:
            LOG("-------------- Application CleanUp --------------");
            if (App->CleanUp())
                mainReturn = EXIT_SUCCESS;
            else
                LOG("Application CleanUp exits with ERROR");
            state = MainState::Exit;
            break;

        case MainState::Exit:
            break;
        }
    }

    RELEASE(App);
    LOG("Exiting engine ...");
    return mainReturn;
}
