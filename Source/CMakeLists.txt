file(GLOB EGE_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
file(GLOB EGE_ENGINE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB EGE_PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Project/*.cpp")
file(GLOB EGE_PROJECT_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Project/*.h")
file(GLOB EGE_SETTINGS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Settings/*.cpp")
file(GLOB EGE_SETTINGS_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Settings/*.h")

list(APPEND EGE_ENGINE_SOURCES
    ${EGE_PROJECT_SOURCES}
    ${EGE_SETTINGS_SOURCES})
list(APPEND EGE_ENGINE_HEADERS
    ${EGE_PROJECT_HEADERS}
    ${EGE_SETTINGS_HEADERS})

set(EGE_EDITOR_ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/EditorMain.cpp")
set(EGE_RUNTIME_ENTRY "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeMain.cpp")

# Remove the obsolete monolithic executable left behind by pre-DLL build
# directories. Current builds only produce Engine.dll, Editor.exe and
# Runtime.exe.
foreach(EGE_CONFIGURATION Debug Release RelWithDebInfo MinSizeRel)
    file(REMOVE "${PROJECT_BINARY_DIR}/bin/${EGE_CONFIGURATION}/Engine.exe")
endforeach()

# Application entry points are separate from the engine DLL. This unfinished
# component was also not part of the original Visual Studio build.
list(REMOVE_ITEM EGE_ENGINE_SOURCES
    "${EGE_EDITOR_ENTRY}"
    "${EGE_RUNTIME_ENTRY}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ComponentLight.cpp")
list(APPEND EGE_ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/gpudetect/DeviceId.cpp")

add_library(Engine SHARED
    ${EGE_ENGINE_SOURCES}
    ${EGE_ENGINE_HEADERS})

target_compile_features(Engine PRIVATE cxx_std_20)
target_compile_definitions(Engine
    PRIVATE
        EGE_ENGINE_EXPORTS
        _CRT_SECURE_NO_WARNINGS
        _MBCS
        GLEW_STATIC
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

target_include_directories(Engine
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/utils"
        "${CMAKE_CURRENT_SOURCE_DIR}/gpudetect"
        "${CMAKE_CURRENT_SOURCE_DIR}/imgui-filebrowser"
        "${bullet_SOURCE_DIR}/src"
        "${physfs_SOURCE_DIR}/src")

target_link_libraries(Engine
    PRIVATE
        SDL2-static
        EGE::GLEW
        assimp::assimp
        BulletDynamics
        BulletCollision
        LinearMath
        EGE::PhysFS
        DirectXTex
        EGE::ImGui
        EGE::TinyGLTF
        EGE::miniaudio
        EGE::MathGeoLib
        EGE::TheklaAtlas
        EGE::TinySpline
        EGE::NodeEditor
        opengl32
        glu32
        dbghelp
        shlwapi)

add_executable(Editor WIN32 "${EGE_EDITOR_ENTRY}")
add_executable(Runtime WIN32 "${EGE_RUNTIME_ENTRY}")
target_compile_features(Editor PRIVATE cxx_std_20)
target_compile_features(Runtime PRIVATE cxx_std_20)
target_link_libraries(Editor PRIVATE Engine)
target_link_libraries(Runtime PRIVATE Engine)

if(MSVC)
    target_compile_options(Engine PRIVATE
        /MP
        /W3
        /arch:AVX
        $<$<BOOL:${EGE_WARNINGS_AS_ERRORS}>:/WX>)

    set_property(DIRECTORY "${PROJECT_SOURCE_DIR}"
        PROPERTY VS_STARTUP_PROJECT Editor)
    set_target_properties(Editor Runtime PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/Game")
endif()

set_target_properties(Engine Editor Runtime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set_target_properties(Engine PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib"
    OUTPUT_NAME "Engine")
set_target_properties(Editor PROPERTIES OUTPUT_NAME "Editor")
set_target_properties(Runtime PROPERTIES OUTPUT_NAME "Runtime")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    PREFIX "Engine"
    FILES ${EGE_ENGINE_SOURCES} ${EGE_ENGINE_HEADERS})
source_group("Entry points" FILES "${EGE_EDITOR_ENTRY}" "${EGE_RUNTIME_ENTRY}")
