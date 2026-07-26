#ifndef EGE_ENGINE_API_H
#define EGE_ENGINE_API_H

#if defined(_WIN32)
    #if defined(EGE_ENGINE_EXPORTS)
        #define EGE_API __declspec(dllexport)
    #else
        #define EGE_API __declspec(dllimport)
    #endif
#else
    #define EGE_API
#endif

enum class EngineMode
{
    Editor,
    Runtime
};

EGE_API int RunEngine(EngineMode mode);

#endif
