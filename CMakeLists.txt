cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DEngine VERSION 0.1.0 LANGUAGES CXX)

option(L2D_BUILD_SANDBOX "Build the Lorenzo2D sandbox application" ON)
option(L2D_BUILD_TESTS "Build the Lorenzo2D regression tests" ON)
option(L2D_USE_SYSTEM_SFML "Use an installed SFML 3 package instead of FetchContent" OFF)
option(L2D_WARNINGS_AS_ERRORS "Treat Lorenzo2D compiler warnings as errors" OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

function(l2d_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(L2D_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
        )
        if(L2D_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(L2D_USE_SYSTEM_SFML)
    find_package(SFML 3.1 CONFIG REQUIRED COMPONENTS Graphics)
else()
    include(FetchContent)

    # Lorenzo2D currently uses only SFML Graphics. Disabling unused modules keeps
    # first-time configuration smaller and avoids unrelated audio dependencies.
    set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM
    )

    FetchContent_MakeAvailable(SFML)
endif()

set(LORENZO2D_SOURCES
    src/Lorenzo2D/Assets/AssetManager.cpp
    src/Lorenzo2D/Core/ActionMap.cpp
    src/Lorenzo2D/Core/Application.cpp
    src/Lorenzo2D/Core/FixedStepScheduler.cpp
    src/Lorenzo2D/Core/Input.cpp
    src/Lorenzo2D/Core/Mouse.cpp
    src/Lorenzo2D/Core/Time.cpp
    src/Lorenzo2D/Core/WindowEvents.cpp
    src/Lorenzo2D/ECS/Component.cpp
    src/Lorenzo2D/ECS/GameObject.cpp
    src/Lorenzo2D/ECS/Transform.cpp
    src/Lorenzo2D/Physics/BoxCollider2D.cpp
    src/Lorenzo2D/Physics/CircleCollider2D.cpp
    src/Lorenzo2D/Physics/Collider2D.cpp
    src/Lorenzo2D/Physics/CollisionManifold2D.cpp
    src/Lorenzo2D/Physics/PhysicsDebugRenderer2D.cpp
    src/Lorenzo2D/Physics/PhysicsWorld2D.cpp
    src/Lorenzo2D/Physics/RigidBody2D.cpp
    src/Lorenzo2D/Physics/UniformGridBroadPhase2D.cpp
    src/Lorenzo2D/Renderer/Camera2D.cpp
    src/Lorenzo2D/Renderer/CircleRenderer.cpp
    src/Lorenzo2D/Renderer/DebugOverlay.cpp
    src/Lorenzo2D/Renderer/OrthographicCameraController2D.cpp
    src/Lorenzo2D/Renderer/RectangleRenderer.cpp
    src/Lorenzo2D/Renderer/RenderLayerStack2D.cpp
    src/Lorenzo2D/Renderer/SpriteRenderer.cpp
    src/Lorenzo2D/Scene/GameObjectHandle.cpp
    src/Lorenzo2D/Scene/Scene.cpp
    src/Lorenzo2D/Scene/SceneManager.cpp
    src/Lorenzo2D/Tilemap/Tilemap.cpp
)

add_library(Lorenzo2D STATIC ${LORENZO2D_SOURCES})
add_library(Lorenzo2D::Lorenzo2D ALIAS Lorenzo2D)

target_include_directories(Lorenzo2D
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

target_compile_features(Lorenzo2D PUBLIC cxx_std_17)
target_link_libraries(Lorenzo2D PUBLIC SFML::Graphics)
l2d_enable_warnings(Lorenzo2D)

if(L2D_BUILD_SANDBOX)
    add_executable(Sandbox sandbox/main.cpp)

    target_compile_definitions(Sandbox
        PRIVATE
            L2D_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )

    target_link_libraries(Sandbox PRIVATE Lorenzo2D::Lorenzo2D)
    l2d_enable_warnings(Sandbox)

    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Sandbox)
    set_target_properties(Sandbox PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

if(L2D_BUILD_TESTS)
    enable_testing()

    add_executable(Lorenzo2DTests tests/Lorenzo2DTests.cpp)
    target_link_libraries(Lorenzo2DTests PRIVATE Lorenzo2D::Lorenzo2D)
    l2d_enable_warnings(Lorenzo2DTests)

    add_test(NAME Lorenzo2DTests COMMAND Lorenzo2DTests)

    add_executable(
        Lorenzo2DPhysicsTests
        tests/Lorenzo2DPhysicsTests.cpp
    )
    target_link_libraries(
        Lorenzo2DPhysicsTests
        PRIVATE Lorenzo2D::Lorenzo2D
    )
    l2d_enable_warnings(Lorenzo2DPhysicsTests)

    add_test(NAME Lorenzo2DPhysicsTests COMMAND Lorenzo2DPhysicsTests)

    add_executable(
        Lorenzo2DAssetTests
        tests/Lorenzo2DAssetTests.cpp
    )
    target_link_libraries(
        Lorenzo2DAssetTests
        PRIVATE Lorenzo2D::Lorenzo2D
    )
    l2d_enable_warnings(Lorenzo2DAssetTests)

    add_test(NAME Lorenzo2DAssetTests COMMAND Lorenzo2DAssetTests)

    add_executable(
        Lorenzo2DRendererTests
        tests/Lorenzo2DRendererTests.cpp
    )
    target_link_libraries(
        Lorenzo2DRendererTests
        PRIVATE Lorenzo2D::Lorenzo2D
    )
    target_include_directories(
        Lorenzo2DRendererTests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/Lorenzo2D/Renderer
    )
    l2d_enable_warnings(Lorenzo2DRendererTests)

    add_test(NAME Lorenzo2DRendererTests COMMAND Lorenzo2DRendererTests)
    set_tests_properties(
        Lorenzo2DRendererTests
        PROPERTIES LABELS headless
    )
endif()
