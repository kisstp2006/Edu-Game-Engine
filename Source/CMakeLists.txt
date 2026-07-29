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
file(GLOB EGE_SCRIPTING_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/*.cpp")
file(GLOB EGE_SCRIPTING_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/*.h")
file(GLOB EGE_REFLECTION_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Reflection/*.cpp")
file(GLOB EGE_REFLECTION_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Reflection/*.h")

list(APPEND EGE_ENGINE_SOURCES
    ${EGE_PROJECT_SOURCES}
    ${EGE_SETTINGS_SOURCES}
    ${EGE_SCRIPTING_SOURCES}
    ${EGE_REFLECTION_SOURCES})
list(APPEND EGE_ENGINE_HEADERS
    ${EGE_PROJECT_HEADERS}
    ${EGE_SETTINGS_HEADERS}
    ${EGE_SCRIPTING_HEADERS}
    ${EGE_REFLECTION_HEADERS})

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
        EGE::AngelScriptAddons
        EGE::MathGeoLib
        EGE::TheklaAtlas
        EGE::TinySpline
        EGE::NodeEditor
        opengl32
        glu32
        shell32
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
set_target_properties(Engine PROPERTIES FOLDER "Engine")
set_target_properties(Editor Runtime PROPERTIES FOLDER "Applications")
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

if(EGE_BUILD_TESTS)
	add_executable(ReziAudioFoundationTests
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioTypes.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/miniaudio_impl.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReziAudioFoundationTests.cpp")
	target_compile_features(ReziAudioFoundationTests PRIVATE cxx_std_20)
	target_compile_definitions(ReziAudioFoundationTests PRIVATE NOMINMAX)
	target_link_libraries(
		ReziAudioFoundationTests
		PRIVATE
			EGE::miniaudio
			EGE::MathGeoLib)
	set_target_properties(ReziAudioFoundationTests PROPERTIES
		FOLDER "Tests/Audio"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ReziAudioFoundation
		COMMAND ReziAudioFoundationTests)

	add_executable(ReziAudioGraphTests
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioGraph.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioGraph.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioTypes.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReziAudioGraphTests.cpp")
	target_compile_features(ReziAudioGraphTests PRIVATE cxx_std_20)
	target_compile_definitions(ReziAudioGraphTests PRIVATE NOMINMAX)
	target_link_libraries(
		ReziAudioGraphTests
		PRIVATE
			EGE::MathGeoLib)
	set_target_properties(ReziAudioGraphTests PROPERTIES
		FOLDER "Tests/Audio"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ReziAudioGraph
		COMMAND ReziAudioGraphTests)

	add_executable(ReziAudioDspGraphTests
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioDspGraph.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioDspGraph.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioTypes.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/miniaudio_impl.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReziAudioDspGraphTests.cpp")
	target_compile_features(ReziAudioDspGraphTests PRIVATE cxx_std_20)
	target_compile_definitions(ReziAudioDspGraphTests PRIVATE NOMINMAX)
	target_link_libraries(
		ReziAudioDspGraphTests
		PRIVATE
			EGE::miniaudio
			EGE::MathGeoLib)
	set_target_properties(ReziAudioDspGraphTests PROPERTIES
		FOLDER "Tests/Audio"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ReziAudioDspGraph
		COMMAND ReziAudioDspGraphTests)

	add_executable(ReziAudioLab
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioTypes.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/miniaudio_impl.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReziAudioLab.cpp")
	target_compile_features(ReziAudioLab PRIVATE cxx_std_20)
	target_compile_definitions(
		ReziAudioLab
		PRIVATE
			NOMINMAX
			EGE_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
	target_link_libraries(
		ReziAudioLab
		PRIVATE
			SDL2-static
			EGE::GLEW
			EGE::ImGui
			EGE::miniaudio
			EGE::MathGeoLib
			opengl32)
	set_target_properties(ReziAudioLab PROPERTIES
		FOLDER "Tests/Interactive"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ReziAudioLabSmoke
		COMMAND ReziAudioLab --smoke)

	add_executable(ReziAudioGraphLab
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioGraph.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioGraph.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioMiniaudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioSystem.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioBackend.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/ReziAudioTypes.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/miniaudio_impl.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ReziAudioGraphLab.cpp")
	target_compile_features(ReziAudioGraphLab PRIVATE cxx_std_20)
	target_compile_definitions(
		ReziAudioGraphLab
		PRIVATE
			NOMINMAX
			EGE_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
	target_link_libraries(
		ReziAudioGraphLab
		PRIVATE
			SDL2-static
			EGE::GLEW
			EGE::ImGui
			EGE::NodeEditor
			EGE::miniaudio
			EGE::MathGeoLib
			opengl32)
	set_target_properties(ReziAudioGraphLab PROPERTIES
		FOLDER "Tests/Interactive"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ReziAudioGraphLabSmoke
		COMMAND ReziAudioGraphLab --smoke)

    add_executable(ScriptingBackendTests
        "${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Config.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/parson.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/parson.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/EngineTime.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/EngineTime.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Project/VsCodeWorkspace.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Project/VsCodeWorkspace.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Reflection/PropertyConfigSerializer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Reflection/PropertySerializer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Reflection/TypeRegistry.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptAsset.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptAsset.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptApiRegistry.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptApiRegistry.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptCoreHelpers.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptCoreHelpers.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptInstanceContext.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptInstanceContext.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptMath.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptMath.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptExtendedMath.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptPhysics.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptPhysics.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptResource.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptResource.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptTime.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptTime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptRuntime.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Scripting/ScriptRuntime.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/ScriptRuntimeTests.cpp")
    target_compile_features(ScriptingBackendTests PRIVATE cxx_std_20)
    target_compile_definitions(ScriptingBackendTests
        PRIVATE
			EGE_SOURCE_ROOT="${PROJECT_SOURCE_DIR}"
			NOMINMAX)
    target_link_libraries(ScriptingBackendTests
        PRIVATE
            EGE::AngelScriptAddons
            EGE::MathGeoLib
            shell32)
    set_target_properties(ScriptingBackendTests PROPERTIES
        FOLDER "Tests/Scripting"
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

    add_test(
        NAME ScriptingBackend
        COMMAND ScriptingBackendTests)

	add_executable(ImportSettingsTests
		"${CMAKE_CURRENT_SOURCE_DIR}/ImportSettings.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/ImportSettings.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ImportSettingsTests.cpp")
	target_compile_features(ImportSettingsTests PRIVATE cxx_std_20)
	target_link_libraries(ImportSettingsTests
		PRIVATE
			EGE::ImGui
			EGE::MathGeoLib
			opengl32)
	set_target_properties(ImportSettingsTests PROPERTIES
		FOLDER "Tests/Assets"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ImportSettings
		COMMAND ImportSettingsTests)

	add_executable(EditorHistoryTests
		"${CMAKE_CURRENT_SOURCE_DIR}/EditorHistory.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/EditorHistory.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/EditorHistoryTests.cpp")
	target_compile_features(EditorHistoryTests PRIVATE cxx_std_20)
	set_target_properties(EditorHistoryTests PROPERTIES
		FOLDER "Tests/Editor"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME EditorHistory
		COMMAND EditorHistoryTests)

	add_executable(ModelImportCoordinatesTests
		"${CMAKE_CURRENT_SOURCE_DIR}/ModelImportCoordinates.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/ModelImportCoordinatesTests.cpp")
	target_compile_features(
		ModelImportCoordinatesTests PRIVATE cxx_std_20)
	target_compile_definitions(
		ModelImportCoordinatesTests PRIVATE NOMINMAX)
	target_link_libraries(
		ModelImportCoordinatesTests PRIVATE EGE::MathGeoLib)
	set_target_properties(ModelImportCoordinatesTests PROPERTIES
		FOLDER "Tests/Assets"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME ModelImportCoordinates
		COMMAND ModelImportCoordinatesTests)

	add_executable(PhysicsCollisionShapeTests
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollisionShape.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollisionShape.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsQuery.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsQuery.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/PhysicsCollisionShapeTests.cpp")
	target_compile_features(
		PhysicsCollisionShapeTests PRIVATE cxx_std_20)
	target_compile_definitions(
		PhysicsCollisionShapeTests PRIVATE NOMINMAX)
	target_include_directories(
		PhysicsCollisionShapeTests PRIVATE "${bullet_SOURCE_DIR}/src")
	target_link_libraries(
		PhysicsCollisionShapeTests
		PRIVATE
			BulletDynamics
			BulletCollision
			LinearMath
			EGE::MathGeoLib)
	set_target_properties(PhysicsCollisionShapeTests PROPERTIES
		FOLDER "Tests/Physics"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME PhysicsCollisionShape
		COMMAND PhysicsCollisionShapeTests)

	add_executable(PhysicsInteractionTests
		"${CMAKE_CURRENT_SOURCE_DIR}/Config.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Config.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/parson.c"
		"${CMAKE_CURRENT_SOURCE_DIR}/parson.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollision.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsColliderData.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsColliderData.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollisionMatrix.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollisionMatrix.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollisionSettings.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsCollisionSettings.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsContactTracker.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/PhysicsContactTracker.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Settings/SettingsStore.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/Settings/SettingsStore.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/PhysicsInteractionTests.cpp")
	target_compile_features(
		PhysicsInteractionTests PRIVATE cxx_std_20)
	target_compile_definitions(
		PhysicsInteractionTests
		PRIVATE
			NOMINMAX
			EGE_SOURCE_ROOT="${PROJECT_SOURCE_DIR}")
	target_include_directories(
		PhysicsInteractionTests PRIVATE "${bullet_SOURCE_DIR}/src")
	target_link_libraries(
		PhysicsInteractionTests
		PRIVATE
			BulletDynamics
			BulletCollision
			LinearMath
			EGE::MathGeoLib)
	set_target_properties(PhysicsInteractionTests PROPERTIES
		FOLDER "Tests/Physics"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME PhysicsInteraction
		COMMAND PhysicsInteractionTests)

	add_executable(EngineAngelScriptApiTests
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/EngineAngelScriptApiTests.cpp")
	target_compile_features(
		EngineAngelScriptApiTests PRIVATE cxx_std_20)
	target_compile_definitions(
		EngineAngelScriptApiTests PRIVATE NOMINMAX)
	target_link_libraries(
		EngineAngelScriptApiTests
		PRIVATE
			Engine)
	set_target_properties(EngineAngelScriptApiTests PROPERTIES
		FOLDER "Tests/Scripting"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME EngineAngelScriptApi
		COMMAND EngineAngelScriptApiTests)

	add_executable(PhysicsSceneScriptingE2ETests
		"${CMAKE_CURRENT_SOURCE_DIR}/Tests/PhysicsSceneScriptingE2ETests.cpp")
	target_compile_features(
		PhysicsSceneScriptingE2ETests PRIVATE cxx_std_20)
	target_compile_definitions(
		PhysicsSceneScriptingE2ETests PRIVATE NOMINMAX)
	target_link_libraries(
		PhysicsSceneScriptingE2ETests
		PRIVATE
			Engine)
	set_target_properties(PhysicsSceneScriptingE2ETests PROPERTIES
		FOLDER "Tests/Physics"
		RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
	add_test(
		NAME PhysicsSceneScriptingE2E
		COMMAND PhysicsSceneScriptingE2ETests)
endif()
