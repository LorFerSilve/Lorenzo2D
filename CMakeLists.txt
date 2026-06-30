cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DEngine LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_MakeAvailable(SFML)

file(GLOB_RECURSE LORENZO2D_SOURCES
    CONFIGURE_DEPENDS
    src/Lorenzo2D/*.cpp
)

add_library(Lorenzo2D STATIC
    ${LORENZO2D_SOURCES}
 "src/Lorenzo2D/Core/Input.cpp" "src/Lorenzo2D/Core/Time.cpp" "src/Lorenzo2D/ECS/Transform.cpp" "src/Lorenzo2D/ECS/GameObject.cpp" "src/Lorenzo2D/Scene/Scene.cpp" "src/Lorenzo2D/ECS/Component.cpp" "src/Lorenzo2D/Renderer/CircleRenderer.cpp" "src/Lorenzo2D/Renderer/RectangleRenderer.cpp" "src/Lorenzo2D/Physics/Collider2D.cpp" "src/Lorenzo2D/Physics/BoxCollider2D.cpp" "src/Lorenzo2D/Physics/CircleCollider2D.cpp" "src/Lorenzo2D/Physics/PhysicsWorld2D.cpp" "src/Lorenzo2D/Scene/SceneManager.cpp" "src/Lorenzo2D/Renderer/Camera2D.cpp" "src/Lorenzo2D/Renderer/DebugOverlay.cpp" "src/Lorenzo2D/Assets/AssetManager.cpp" "src/Lorenzo2D/Renderer/SpriteRenderer.cpp" "src/Lorenzo2D/Tilemap/Tilemap.cpp" "src/Lorenzo2D/Physics/PhysicsDebugRenderer2D.cpp" "src/Lorenzo2D/Core/ActionMap.cpp" "src/Lorenzo2D/Core/Mouse.cpp" "src/Lorenzo2D/Core/WindowEvents.cpp" "src/Lorenzo2D/Renderer/OrthographicCameraController2D.cpp" "src/Lorenzo2D/Scene/GameObjectHandle.cpp")

target_include_directories(Lorenzo2D
    PUBLIC
        include
)

target_compile_features(Lorenzo2D
    PUBLIC
        cxx_std_17
)

target_link_libraries(Lorenzo2D
    PUBLIC
        SFML::Graphics
)

add_executable(Sandbox
    sandbox/main.cpp
 "src/Lorenzo2D/Physics/PhysicsDebugRenderer2D.cpp" "src/Lorenzo2D/Core/ActionMap.cpp" "src/Lorenzo2D/Core/Mouse.cpp" "src/Lorenzo2D/Core/WindowEvents.cpp" "src/Lorenzo2D/Renderer/OrthographicCameraController2D.cpp" "src/Lorenzo2D/Scene/GameObjectHandle.cpp")

target_compile_definitions(Sandbox
    PRIVATE
        L2D_ASSET_ROOT="${CMAKE_SOURCE_DIR}/assets"
)

target_link_libraries(Sandbox
    PRIVATE
        Lorenzo2D
)

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Sandbox)

set_target_properties(Sandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)