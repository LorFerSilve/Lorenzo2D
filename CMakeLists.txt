cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DEngine VERSION 0.10.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(cmake/Lorenzo2DDeveloperTools.cmake)

set(L2D_GENERATED_INCLUDE_DIRECTORY
    "${PROJECT_BINARY_DIR}/generated/include"
)
file(MAKE_DIRECTORY
    "${L2D_GENERATED_INCLUDE_DIRECTORY}/Lorenzo2D/Core"
)
configure_file(
    cmake/Lorenzo2DVersion.hpp.in
    "${L2D_GENERATED_INCLUDE_DIRECTORY}/Lorenzo2D/Core/Version.hpp"
    @ONLY
)

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
option(
    L2D_BUILD_EXAMPLES
    "Build the focused Lorenzo2D example applications"
    ${PROJECT_IS_TOP_LEVEL}
)
option(L2D_USE_SYSTEM_SFML "Use an installed SFML 3 package instead of FetchContent" OFF)
option(L2D_WARNINGS_AS_ERRORS "Treat Lorenzo2D compiler warnings as errors" OFF)
option(L2D_ENABLE_ASAN "Instrument first-party targets with AddressSanitizer" OFF)
option(L2D_ENABLE_UBSAN "Instrument first-party targets with UndefinedBehaviorSanitizer" OFF)
option(L2D_ENABLE_COVERAGE "Instrument first-party targets for coverage reporting" OFF)
option(L2D_ENABLE_CLANG_TIDY "Run clang-tidy while compiling first-party targets" OFF)
option(L2D_BUILD_BENCHMARKS "Build the Lorenzo2D benchmark executable" OFF)
option(
    L2D_INSTALL
    "Generate Lorenzo2D installation and CMake package export rules"
    ${PROJECT_IS_TOP_LEVEL}
)

if(L2D_ENABLE_COVERAGE AND NOT L2D_BUILD_TESTS)
    message(FATAL_ERROR "L2D_ENABLE_COVERAGE requires L2D_BUILD_TESTS=ON")
endif()

if(L2D_ENABLE_COVERAGE AND (L2D_ENABLE_ASAN OR L2D_ENABLE_UBSAN))
    message(FATAL_ERROR "Coverage and sanitizer instrumentation use separate build presets")
endif()

l2d_add_format_targets()

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

if(TARGET SFML::Graphics)
    message(STATUS "Using the SFML::Graphics target provided by the parent project")
elseif(L2D_USE_SYSTEM_SFML)
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

if(TARGET nlohmann_json::nlohmann_json)
    message(STATUS "Using the nlohmann_json target provided by the parent project")
else()
    include(FetchContent)

    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
        URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
        EXCLUDE_FROM_ALL
        SYSTEM
    )

    set(JSON_BuildTests OFF CACHE INTERNAL "")
    set(JSON_Install OFF CACHE INTERNAL "")
    FetchContent_MakeAvailable(nlohmann_json)
endif()

set(LORENZO2D_SOURCES
    src/Lorenzo2D/Animation/AnimationClip.cpp
    src/Lorenzo2D/Animation/Animator.cpp
    src/Lorenzo2D/Assets/AssetManager.cpp
    src/Lorenzo2D/Assets/AssetPipeline.cpp
    src/Lorenzo2D/Assets/ResourceLocator.cpp
    src/Lorenzo2D/Core/ActionMap.cpp
    src/Lorenzo2D/Core/Application.cpp
    src/Lorenzo2D/Core/FixedStepScheduler.cpp
    src/Lorenzo2D/Core/Input.cpp
    src/Lorenzo2D/Core/InputContextStack.cpp
    src/Lorenzo2D/Core/InputMap.cpp
    src/Lorenzo2D/Core/InputSnapshot.cpp
    src/Lorenzo2D/Core/Mouse.cpp
    src/Lorenzo2D/Core/Pointer.cpp
    src/Lorenzo2D/Core/Time.cpp
    src/Lorenzo2D/Core/WindowEvents.cpp
    src/Lorenzo2D/ECS/Component.cpp
    src/Lorenzo2D/ECS/GameObject.cpp
    src/Lorenzo2D/ECS/Transform.cpp
    src/Lorenzo2D/Movement/CharacterMotor2D.cpp
    src/Lorenzo2D/Movement/GridStepController2D.cpp
    src/Lorenzo2D/Movement/TopDownController2D.cpp
    src/Lorenzo2D/Physics/BoxCollider2D.cpp
    src/Lorenzo2D/Physics/CapsuleCollider2D.cpp
    src/Lorenzo2D/Physics/CircleCollider2D.cpp
    src/Lorenzo2D/Physics/Collider2D.cpp
    src/Lorenzo2D/Physics/CollisionManifold2D.cpp
    src/Lorenzo2D/Physics/ConvexPolygonCollider2D.cpp
    src/Lorenzo2D/Physics/DistanceJoint2D.cpp
    src/Lorenzo2D/Physics/PhysicsDebugRenderer2D.cpp
    src/Lorenzo2D/Physics/PhysicsGeometry2D.cpp
    src/Lorenzo2D/Physics/PhysicsQueries2D.cpp
    src/Lorenzo2D/Physics/PhysicsWorld2D.cpp
    src/Lorenzo2D/Physics/PhysicsWorld2DContacts.cpp
    src/Lorenzo2D/Physics/PhysicsWorld2DProxies.cpp
    src/Lorenzo2D/Physics/RigidBody2D.cpp
    src/Lorenzo2D/Physics/UniformGridBroadPhase2D.cpp
    src/Lorenzo2D/Renderer/Camera2D.cpp
    src/Lorenzo2D/Renderer/CircleRenderer.cpp
    src/Lorenzo2D/Renderer/DebugOverlay.cpp
    src/Lorenzo2D/Renderer/OrthographicCameraController2D.cpp
    src/Lorenzo2D/Renderer/ParticleEmitter2D.cpp
    src/Lorenzo2D/Renderer/PostProcessStack2D.cpp
    src/Lorenzo2D/Renderer/RectangleRenderer.cpp
    src/Lorenzo2D/Renderer/RenderLayerStack2D.cpp
    src/Lorenzo2D/Renderer/RenderOrder2D.cpp
    src/Lorenzo2D/Renderer/RenderQueue2D.cpp
    src/Lorenzo2D/Renderer/RenderSortKey2D.cpp
    src/Lorenzo2D/Renderer/SpriteRenderer.cpp
    src/Lorenzo2D/Scene/GameObjectHandle.cpp
    src/Lorenzo2D/Scene/ComponentCodecRegistry.cpp
    src/Lorenzo2D/Scene/LevelSerializer.cpp
    src/Lorenzo2D/Scene/Prefab.cpp
    src/Lorenzo2D/Scene/Scene.cpp
    src/Lorenzo2D/Scene/SceneManager.cpp
    src/Lorenzo2D/Tilemap/Tilemap.cpp
    src/Lorenzo2D/Tilemap/AsciiTileMapImporter.cpp
    src/Lorenzo2D/Tilemap/TileMapColliderBuilder2D.cpp
    src/Lorenzo2D/Tilemap/TileMapData.cpp
    src/Lorenzo2D/Tilemap/TileSet.cpp
    src/Lorenzo2D/Tilemap/TiledJsonImporter.cpp
)

add_library(Lorenzo2D STATIC ${LORENZO2D_SOURCES})
add_library(Lorenzo2D::Lorenzo2D ALIAS Lorenzo2D)

set_target_properties(Lorenzo2D PROPERTIES
    EXPORT_NAME Lorenzo2D
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(Lorenzo2D
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${L2D_GENERATED_INCLUDE_DIRECTORY}>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)

get_target_property(
    l2d_json_include_directories
    nlohmann_json::nlohmann_json
    INTERFACE_INCLUDE_DIRECTORIES
)
target_include_directories(Lorenzo2D PRIVATE ${l2d_json_include_directories})

target_compile_features(Lorenzo2D PUBLIC cxx_std_17)
target_link_libraries(Lorenzo2D PUBLIC SFML::Graphics)
l2d_enable_warnings(Lorenzo2D)
l2d_enable_sanitizers(Lorenzo2D)
l2d_enable_coverage(Lorenzo2D)
l2d_enable_clang_tidy(Lorenzo2D)

if(L2D_BUILD_SANDBOX)
    add_executable(Sandbox sandbox/main.cpp)

    target_link_libraries(Sandbox PRIVATE Lorenzo2D::Lorenzo2D)
    l2d_enable_warnings(Sandbox)
    l2d_enable_sanitizers(Sandbox)
    l2d_enable_coverage(Sandbox)
    l2d_enable_clang_tidy(Sandbox)

    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Sandbox)
    set_target_properties(Sandbox PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_custom_command(
        TARGET Sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:Sandbox>/assets"
        VERBATIM
    )
endif()

if(L2D_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(L2D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
    l2d_add_coverage_target()
endif()

if(L2D_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
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

    install(
        FILES
            "${L2D_GENERATED_INCLUDE_DIRECTORY}/Lorenzo2D/Core/Version.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/Lorenzo2D/Core"
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
        COMPATIBILITY SameMinorVersion
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
