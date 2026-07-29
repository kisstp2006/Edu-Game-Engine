include_guard(GLOBAL)

# These libraries are old, source-integrated APIs with local engine-specific
# compatibility changes. They are isolated as targets now, which lets us
# replace them with pinned FetchContent versions independently later.

file(GLOB_RECURSE EGE_MATHGEOLIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/MathGeoLib/include/*.cpp")
add_library(ege_mathgeolib STATIC ${EGE_MATHGEOLIB_SOURCES})
add_library(EGE::MathGeoLib ALIAS ege_mathgeolib)
target_include_directories(ege_mathgeolib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source/MathGeoLib/include")

file(GLOB_RECURSE EGE_THEKLA_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src/nvcore/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src/nvimage/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src/nvmath/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src/nvmesh/*.cpp")
list(APPEND EGE_THEKLA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/extern/poshlib/posh.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src/thekla/thekla_atlas.cpp")
add_library(ege_thekla_atlas STATIC ${EGE_THEKLA_SOURCES})
add_library(EGE::TheklaAtlas ALIAS ege_thekla_atlas)
target_include_directories(ege_thekla_atlas
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/extern/poshlib"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/thekla_atlas/src/nvmesh")
if(MSVC)
    target_compile_definitions(ege_thekla_atlas PRIVATE
        _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

add_library(ege_tinyspline STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/tinyspline/include/tinyspline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/tinyspline/include/tinysplinecpp.cpp")
add_library(EGE::TinySpline ALIAS ege_tinyspline)
target_include_directories(ege_tinyspline
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source/tinyspline/include")

add_library(ege_node_editor STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/NodeEditor/Source/crude_json.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/NodeEditor/Source/imgui_canvas.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/NodeEditor/Source/imgui_node_editor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/NodeEditor/Source/imgui_node_editor_api.cpp")
add_library(EGE::NodeEditor ALIAS ege_node_editor)
target_include_directories(ege_node_editor
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/NodeEditor/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/NodeEditor/Source")
target_link_libraries(ege_node_editor PUBLIC EGE::ImGui)
