include_guard(GLOBAL)

function(ege_set_target_folder folder)
    foreach(target IN LISTS ARGN)
        if(TARGET "${target}")
            set_property(TARGET "${target}" PROPERTY FOLDER "${folder}")
        endif()
    endforeach()
endfunction()

function(ege_configure_solution_folders)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

    ege_set_target_folder("Engine"
        Engine)

    ege_set_target_folder("Applications"
        Editor
        Runtime)

    ege_set_target_folder("Tests/Audio"
        ReziAudioFoundationTests
        ReziAudioGraphTests
        ReziAudioDspGraphTests)
    ege_set_target_folder("Tests/Interactive"
        ReziAudioLab
        ReziAudioGraphLab)
    ege_set_target_folder("Tests/Scripting"
        ScriptingBackendTests
        EngineAngelScriptApiTests)
    ege_set_target_folder("Tests/Physics"
        PhysicsCollisionShapeTests
        PhysicsInteractionTests
        PhysicsSceneScriptingE2ETests)
    ege_set_target_folder("Tests/Editor"
        EditorHistoryTests)
    ege_set_target_folder("Tests/Assets"
        ImportSettingsTests
        ModelImportCoordinatesTests)

    ege_set_target_folder("Third Party/UI"
        ege_imgui
        ege_node_editor)

    ege_set_target_folder("Third Party/Scripting/AngelScript"
        angelscript
        AngelScript
        ege_angelscript_addons)

    ege_set_target_folder("Third Party/Platform/SDL"
        SDL2
        SDL2-static
        SDL2main
        SDL2_test
        uninstall)

    ege_set_target_folder("Third Party/Graphics/GLEW"
        glew
        glew_s
        glew_shared
        glew_static
        glewinfo
        visualinfo)

    ege_set_target_folder("Third Party/Graphics/DirectXTex"
        DirectXTex)

    ege_set_target_folder("Third Party/Assets/Assimp"
        assimp
        zlibstatic
        UpdateAssimpLibsDebugSymbolsAndDLLs)

    ege_set_target_folder("Third Party/Assets/Thekla Atlas"
        ege_thekla_atlas)

    ege_set_target_folder("Third Party/Physics/Bullet"
        Bullet2FileLoader
        Bullet3Collision
        Bullet3Common
        Bullet3Dynamics
        Bullet3Geometry
        Bullet3OpenCL_clew
        BulletCollision
        BulletDynamics
        BulletInverseDynamics
        BulletSoftBody
        LinearMath)

    ege_set_target_folder("Third Party/File System/PhysicsFS"
        physfs-static
        PhysFS-static
        docs)

    ege_set_target_folder("Third Party/Math"
        ege_mathgeolib
        ege_tinyspline)
endfunction()
