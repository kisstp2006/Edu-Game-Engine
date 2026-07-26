file(GLOB EGE_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
# This unfinished component was not part of the original Visual Studio build.
list(REMOVE_ITEM EGE_ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/ComponentLight.cpp")
list(APPEND EGE_ENGINE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/gpudetect/DeviceId.cpp")

add_executable(Engine WIN32 ${EGE_ENGINE_SOURCES})

target_compile_features(Engine PRIVATE cxx_std_20)
target_compile_definitions(Engine
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _MBCS
        GLEW_STATIC
        NOMINMAX
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
        SDL2main
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

if(MSVC)
    target_compile_options(Engine PRIVATE
        /MP
        /W3
        /arch:AVX
        $<$<BOOL:${EGE_WARNINGS_AS_ERRORS}>:/WX>)

    set_property(DIRECTORY "${PROJECT_SOURCE_DIR}"
        PROPERTY VS_STARTUP_PROJECT Engine)
    set_target_properties(Engine PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/Game")
endif()

set_target_properties(Engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
    OUTPUT_NAME "Engine")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}"
    PREFIX "Engine"
    FILES ${EGE_ENGINE_SOURCES})
