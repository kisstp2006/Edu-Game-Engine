cmake_minimum_required(VERSION 3.24)

# A few of the pinned legacy dependencies still declare an old CMake version.
# This keeps them configurable with modern CMake without weakening our own
# policy settings.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING
    "Minimum policy version used by legacy third-party projects")

project(EduGameEngine VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "Edu Game Engine currently supports Windows only.")
endif()

option(EGE_BUILD_ENGINE "Build the editor/engine executable" ON)
option(EGE_WARNINGS_AS_ERRORS "Treat Edu Game Engine warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_DEBUG_POSTFIX "" CACHE STRING "" FORCE)

if(MSVC)
    # Match the existing Visual Studio project and keep one CRT model across
    # the engine and every dependency built by FetchContent.
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Dependencies)
include(LegacyThirdParty)

if(EGE_BUILD_ENGINE)
    add_subdirectory(Source)
endif()

