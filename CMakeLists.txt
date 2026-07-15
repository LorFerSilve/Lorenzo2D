cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DEngine VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(
    L2D_BUILD_SANDBOX
    "Build the Lorenzo2D sandbox application"
    ${PROJECT_IS_TOP_LEVEL}
)
option(
    L2D_BUILD_TESTS
    "Build the Lorenzo2D regression tests"
    ${PROJECT_IS_TOP_LEVEL}
)
option(L2D_USE_SYSTEM_SFML "Use an installed SFML 3 package instead of FetchContent" OFF)
option(L2D_WARNINGS_AS_ERRORS "Treat Lorenzo2D compiler warnings as errors" OFF)
option(L2D_ENABLE_ASAN "Instrument first-party targets with AddressSanitizer" OFF)
option(L2D_ENABLE_UBSAN "Instrument first-party targets with UndefinedBehaviorSanitizer" OFF)
option(
    L2D_INSTALL
    "Generate Lorenzo2D installation and CMake package export rules"
    ${PROJECT_IS_TOP_LEVEL}
)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

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

function(l2d_enable_sanitizers target)
    if(NOT L2D_ENABLE_ASAN AND NOT L2D_ENABLE_UBSAN)
        return()
    endif()

    if(MSVC)
        message(FATAL_ERROR
            "L2D_ENABLE_ASAN and L2D_ENABLE_UBSAN currently require GCC, Clang, or AppleClang"
        )
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR
            "Lorenzo2D sanitizer options are unsupported by ${CMAKE_CXX_COMPILER_ID}"
        )
    endif()

    set(l2d_sanitizers)

    if(L2D_ENABLE_ASAN)
        list(APPEND l2d_sanitizers address)
    endif()

    if(L2D_ENABLE_UBSAN)
        list(APPEND l2d_sanitizers undefined)
    endif()

    string(JOIN "," l2d_sanitizer_list ${l2d_sanitizers})
    set(l2d_sanitizer_flag "-fsanitize=${l2d_sanitizer_list}")

    target_compile_options(${target} PRIVATE
        ${l2d_sanitizer_flag}
        -fno-omit-frame-pointer
    )
    target_link_options(${target} PRIVATE ${l2d_sanitizer_flag})
endfunction()

if(L2D_USE_SYSTEM_SFML)
    find_package(SFML 3.1 CONFIG REQUIRED COMPONENTS Graphics)
else()
    include(FetchContent)

    # Lorenzo2D currently uses only SFML Graphics. Respect values already chosen
    # by a parent project instead of force-writing shared cache entries.
    if(NOT DEFINED SFML_BUILD_AUDIO)
        set(SFML_BUILD_AUDIO OFF CACHE BOOL "Build SFML Audio")
    endif()
    if(NOT DEFINED SFML_BUILD_NETWORK)
        set(SFML_BUILD_NETWORK OFF CACHE BOOL "Build SFML Network")
    endif()
    if(NOT DEFINED SFML_BUILD_EXAMPLES)
        set(SFML_BUILD_EXAMPLES OFF CACHE BOOL "Build SFML examples")
    endif()
    if(NOT DEFINED SFML_BUILD_TEST_SUITE)
        set(SFML_BUILD_TEST_SUITE OFF CACHE BOOL "Build SFML test suite")
    endif()

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
    src/Lorenzo2D/Physics/PhysicsWorld2DContacts.cpp
    src/Lorenzo2D/Physics/PhysicsWorld2DProxies.cpp
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

set_target_properties(Lorenzo2D PROPERTIES EXPORT_NAME Lorenzo2D)

target_include_directories(Lorenzo2D
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

target_compile_features(Lorenzo2D PUBLIC cxx_std_17)
target_link_libraries(Lorenzo2D PUBLIC SFML::Graphics)
l2d_enable_warnings(Lorenzo2D)
l2d_enable_sanitizers(Lorenzo2D)

if(L2D_BUILD_SANDBOX)
    add_executable(Sandbox sandbox/main.cpp)

    target_compile_definitions(Sandbox
        PRIVATE
            L2D_ASSET_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )

    target_link_libraries(Sandbox PRIVATE Lorenzo2D::Lorenzo2D)
    l2d_enable_warnings(Sandbox)
    l2d_enable_sanitizers(Sandbox)

    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Sandbox)
    set_target_properties(Sandbox PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()

if(L2D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(L2D_INSTALL)
    install(
        TARGETS Lorenzo2D
        EXPORT Lorenzo2DTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(
        DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    get_target_property(
        l2d_sfml_graphics_target
        SFML::Graphics
        ALIASED_TARGET
    )
    if(NOT l2d_sfml_graphics_target)
        set(l2d_sfml_graphics_target SFML::Graphics)
    endif()

    get_target_property(
        l2d_sfml_graphics_type
        ${l2d_sfml_graphics_target}
        TYPE
    )

    if(l2d_sfml_graphics_type STREQUAL "STATIC_LIBRARY")
        set(L2D_PACKAGE_USES_STATIC_SFML ON)
    else()
        set(L2D_PACKAGE_USES_STATIC_SFML OFF)
    endif()

    set(L2D_PACKAGE_INSTALL_DIRECTORY
        "${CMAKE_INSTALL_LIBDIR}/cmake/Lorenzo2D"
    )

    configure_package_config_file(
        cmake/Lorenzo2DConfig.cmake.in
        "${PROJECT_BINARY_DIR}/Lorenzo2DConfig.cmake"
        INSTALL_DESTINATION ${L2D_PACKAGE_INSTALL_DIRECTORY}
    )

    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/Lorenzo2DConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(
        FILES
            "${PROJECT_BINARY_DIR}/Lorenzo2DConfig.cmake"
            "${PROJECT_BINARY_DIR}/Lorenzo2DConfigVersion.cmake"
        DESTINATION ${L2D_PACKAGE_INSTALL_DIRECTORY}
    )

    install(
        EXPORT Lorenzo2DTargets
        FILE Lorenzo2DTargets.cmake
        NAMESPACE Lorenzo2D::
        DESTINATION ${L2D_PACKAGE_INSTALL_DIRECTORY}
    )
endif()
