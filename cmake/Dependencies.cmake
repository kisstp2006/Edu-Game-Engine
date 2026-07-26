include_guard(GLOBAL)
include(FetchContent)

# Do not contact remotes again after a dependency has been populated in this
# build tree. Delete the build directory (or its _deps folder) for a clean
# dependency refresh.
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET ON)

# Build static dependencies so Engine.exe has no third-party DLL deployment
# step. The exact revisions intentionally track the APIs used by this project.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# SDL 2.0.16
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(LIBC ON CACHE BOOL "Use the platform C library in SDL" FORCE)
FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.0.16
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

# GLEW 2.1.0. Its CMake project lives below the repository root.
set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(glew-cmake_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ONLY_LIBS ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    glew
    # The git tag does not contain GLEW's generated headers/sources; the
    # official release archive does.
    URL https://github.com/nigels-com/glew/releases/download/glew-2.1.0/glew-2.1.0.tgz
    URL_HASH SHA256=04de91e7e6763039bc11940095cd9c7f880baba82196a7765f727ac05a993c95
    SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(glew)

# Assimp 5.2.5
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.2.5
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(assimp)

# Bullet 3.17
set(BUILD_BULLET2_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_CPU_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_OPENGL3_DEMOS OFF CACHE BOOL "" FORCE)
set(BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(INSTALL_LIBS OFF CACHE BOOL "" FORCE)
set(USE_MSVC_RUNTIME_LIBRARY_DLL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    bullet
    GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
    GIT_TAG 3.17
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(bullet)

# PhysicsFS 3.0.2
set(PHYSFS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PHYSFS_BUILD_DOCS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    physfs
    GIT_REPOSITORY https://github.com/icculus/physfs.git
    GIT_TAG release-3.0.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(physfs)

# DirectXTex March 2024 release. Only the core texture library is required.
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_SAMPLE OFF CACHE BOOL "" FORCE)
set(BUILD_DX11 OFF CACHE BOOL "" FORCE)
set(BUILD_DX12 OFF CACHE BOOL "" FORCE)
set(BC_USE_OPENMP OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    directxtex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG mar2024
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(directxtex)

# Header/source-only dependencies. SOURCE_SUBDIR points at a deliberately
# absent directory so FetchContent populates them without adding upstream
# examples or tools to our generated solution.
FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.8.10
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _ege_no_subdirectory
)
FetchContent_MakeAvailable(tinygltf)

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR _ege_no_subdirectory
)
FetchContent_MakeAvailable(miniaudio)

# This project uses a customized Dear ImGui 1.80 WIP docking snapshot. The
# stable v1.80 tag does not contain the required docking/viewport API, so this
# dependency remains source-integrated and isolated behind one target.
add_library(ege_imgui STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui_demo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/backends/imgui_impl_opengl3.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/backends/imgui_impl_sdl.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/ImGuizmo.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui_bezier.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui_color_gradient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/imgui_user2.cpp"
)
add_library(EGE::ImGui ALIAS ege_imgui)
target_include_directories(ege_imgui
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/Imgui/backends"
)
target_link_libraries(ege_imgui PUBLIC SDL2-static)
set_target_properties(ege_imgui PROPERTIES FOLDER "Legacy dependencies")

add_library(ege_tinygltf INTERFACE)
add_library(EGE::TinyGLTF ALIAS ege_tinygltf)
target_include_directories(ege_tinygltf INTERFACE "${tinygltf_SOURCE_DIR}")

add_library(ege_miniaudio INTERFACE)
add_library(EGE::miniaudio ALIAS ege_miniaudio)
target_include_directories(ege_miniaudio INTERFACE "${miniaudio_SOURCE_DIR}")

# Normalize dependency target names used by the engine.
if(TARGET libglew_static)
    add_library(EGE::GLEW ALIAS libglew_static)
elseif(TARGET glew_s)
    add_library(EGE::GLEW ALIAS glew_s)
else()
    message(FATAL_ERROR "Could not find the static GLEW target")
endif()

if(TARGET physfs-static)
    add_library(EGE::PhysFS ALIAS physfs-static)
elseif(TARGET PhysFS::PhysFS-static)
    add_library(EGE::PhysFS ALIAS PhysFS::PhysFS-static)
else()
    message(FATAL_ERROR "Could not find the static PhysicsFS target")
endif()
