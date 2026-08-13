include(CMakeParseArguments)

function(l2d_add_regression_test target)
    set(options)
    set(oneValueArguments SOURCE TIMEOUT)
    set(multiValueArguments LABELS PRIVATE_INCLUDE_DIRECTORIES)

    cmake_parse_arguments(
        L2D_TEST
        "${options}"
        "${oneValueArguments}"
        "${multiValueArguments}"
        ${ARGN}
    )

    if(L2D_TEST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "Unexpected arguments for ${target}: ${L2D_TEST_UNPARSED_ARGUMENTS}"
        )
    endif()

    if(NOT L2D_TEST_SOURCE)
        message(FATAL_ERROR "${target} must declare exactly one SOURCE")
    endif()

    if(NOT L2D_TEST_LABELS)
        message(FATAL_ERROR "${target} must declare CTest LABELS")
    endif()

    list(FIND L2D_TEST_LABELS headless l2d_headless_label_index)
    list(FIND L2D_TEST_LABELS xvfb l2d_xvfb_label_index)

    if(
        l2d_headless_label_index LESS 0 AND
        l2d_xvfb_label_index LESS 0
    )
        message(FATAL_ERROR
            "${target} must use either the headless or xvfb runtime label"
        )
    endif()

    if(
        l2d_headless_label_index GREATER_EQUAL 0 AND
        l2d_xvfb_label_index GREATER_EQUAL 0
    )
        message(FATAL_ERROR
            "${target} cannot use both the headless and xvfb runtime labels"
        )
    endif()

    if(NOT L2D_TEST_TIMEOUT)
        set(L2D_TEST_TIMEOUT 120)
    endif()

    add_executable(${target} ${L2D_TEST_SOURCE})
    target_link_libraries(${target} PRIVATE Lorenzo2D::Lorenzo2D)

    if(L2D_TEST_PRIVATE_INCLUDE_DIRECTORIES)
        target_include_directories(
            ${target}
            PRIVATE ${L2D_TEST_PRIVATE_INCLUDE_DIRECTORIES}
        )
    endif()

    l2d_enable_warnings(${target})
    l2d_enable_sanitizers(${target})
    l2d_enable_coverage(${target})
    l2d_enable_clang_tidy(${target})

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target}
        PROPERTIES
            LABELS "${L2D_TEST_LABELS}"
            TIMEOUT ${L2D_TEST_TIMEOUT}
    )
endfunction()

l2d_add_regression_test(
    Lorenzo2DTestSupportTests
    SOURCE TestSupportTests.cpp
    LABELS headless core test-support determinism
    TIMEOUT 30
)

l2d_add_regression_test(
    Lorenzo2DVersionTests
    SOURCE Lorenzo2DVersionTests.cpp
    LABELS headless core version compatibility
    TIMEOUT 30
)

# The former broad regression executable is split by subsystem so a failure maps
# directly to a focused source file and CTest target.
l2d_add_regression_test(
    Lorenzo2DCoreTimingTests
    SOURCE Lorenzo2DCoreTimingTests.cpp
    LABELS headless core timing ecs
    TIMEOUT 60
)

l2d_add_regression_test(
    Lorenzo2DInputTests
    SOURCE Lorenzo2DInputTests.cpp
    LABELS headless core input
    PRIVATE_INCLUDE_DIRECTORIES
        "${PROJECT_SOURCE_DIR}/src/Lorenzo2D/Core"
    TIMEOUT 30
)

l2d_add_regression_test(
    Lorenzo2DEcsSceneTests
    SOURCE Lorenzo2DEcsSceneTests.cpp
    LABELS headless ecs scene
    TIMEOUT 90
)

l2d_add_regression_test(
    Lorenzo2DPhysicsIntegrationTests
    SOURCE Lorenzo2DPhysicsIntegrationTests.cpp
    LABELS headless physics ecs scene timing integration
    TIMEOUT 90
)

l2d_add_regression_test(
    Lorenzo2DTilemapTests
    SOURCE Lorenzo2DTilemapTests.cpp
    LABELS headless tilemap physics renderer
    TIMEOUT 90
)

l2d_add_regression_test(
    Lorenzo2DTimingAccountingTests
    SOURCE Lorenzo2DTimingAccountingTests.cpp
    LABELS headless core timing
    TIMEOUT 30
)

l2d_add_regression_test(
    Lorenzo2DPhysicsBodyTests
    SOURCE physics/Lorenzo2DPhysicsBodyTests.cpp
    LABELS headless physics bodies
    TIMEOUT 90
)

l2d_add_regression_test(
    Lorenzo2DPhysicsCollisionTests
    SOURCE physics/Lorenzo2DPhysicsCollisionTests.cpp
    LABELS headless physics collision
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DPhysicsWorldTests
    SOURCE physics/Lorenzo2DPhysicsWorldTests.cpp
    LABELS headless physics ecs scene
    TIMEOUT 90
)

l2d_add_regression_test(
    Lorenzo2DPhysicsBroadPhaseTests
    SOURCE physics/Lorenzo2DPhysicsBroadPhaseTests.cpp
    LABELS headless physics broad-phase
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DPhysicsQueryTests
    SOURCE physics/Lorenzo2DPhysicsQueryTests.cpp
    LABELS headless physics queries collision determinism
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DCharacterMotorTests
    SOURCE Lorenzo2DCharacterMotorTests.cpp
    LABELS headless movement physics queries determinism
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DTopDownControllerTests
    SOURCE Lorenzo2DTopDownControllerTests.cpp
    LABELS headless movement top-down physics queries determinism
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DGridStepControllerTests
    SOURCE Lorenzo2DGridStepControllerTests.cpp
    LABELS headless movement top-down grid physics queries determinism
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DPlatformerControllerTests
    SOURCE Lorenzo2DPlatformerControllerTests.cpp
    LABELS headless movement platformer physics queries determinism
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DPhysicsTilemapTests
    SOURCE physics/Lorenzo2DPhysicsTilemapTests.cpp
    LABELS headless physics tilemap
    TIMEOUT 90
)

l2d_add_regression_test(
    Lorenzo2DPhysicsStabilityTests
    SOURCE Lorenzo2DPhysicsStabilityTests.cpp
    LABELS headless physics stability determinism
    TIMEOUT 240
)

l2d_add_regression_test(
    Lorenzo2DPhysicsAdvancedTests
    SOURCE physics/Lorenzo2DPhysicsAdvancedTests.cpp
    LABELS headless physics advanced
    TIMEOUT 120
)

l2d_add_regression_test(
    Lorenzo2DAssetTests
    SOURCE Lorenzo2DAssetTests.cpp
    LABELS xvfb assets renderer
    TIMEOUT 60
)

l2d_add_regression_test(
    Lorenzo2DAnimationTests
    SOURCE Lorenzo2DAnimationTests.cpp
    LABELS xvfb animation assets renderer ecs
    TIMEOUT 60
)

l2d_add_regression_test(
    Lorenzo2DResourceTests
    SOURCE Lorenzo2DResourceTests.cpp
    LABELS headless assets resources
    TIMEOUT 30
)

l2d_add_regression_test(
    Lorenzo2DSerializationTests
    SOURCE Lorenzo2DSerializationTests.cpp
    LABELS headless scene prefabs serialization physics renderer
    TIMEOUT 60
)

configure_file(
    "${PROJECT_SOURCE_DIR}/assets/levels/phase2-showcase.l2dlevel"
    "${CMAKE_CURRENT_BINARY_DIR}/phase2-showcase.l2dlevel"
    COPYONLY
)

l2d_add_regression_test(
    Lorenzo2DRendererTests
    SOURCE Lorenzo2DRendererTests.cpp
    LABELS headless renderer
    PRIVATE_INCLUDE_DIRECTORIES
        "${PROJECT_SOURCE_DIR}/src/Lorenzo2D/Renderer"
    TIMEOUT 60
)
