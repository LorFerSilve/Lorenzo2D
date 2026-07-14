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

    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(
        ${target}
        PROPERTIES
            LABELS "${L2D_TEST_LABELS}"
            TIMEOUT ${L2D_TEST_TIMEOUT}
    )
endfunction()

# The broad suite retains its existing coverage and executable name. New
# subsystem-specific regressions should normally go into a focused suite rather
# than making this legacy integration target broader.
l2d_add_regression_test(
    Lorenzo2DTests
    SOURCE Lorenzo2DTests.cpp
    LABELS xvfb core timing ecs scene physics tilemap integration
    TIMEOUT 180
)

l2d_add_regression_test(
    Lorenzo2DTimingAccountingTests
    SOURCE Lorenzo2DTimingAccountingTests.cpp
    LABELS headless core timing
    TIMEOUT 30
)

l2d_add_regression_test(
    Lorenzo2DPhysicsTests
    SOURCE Lorenzo2DPhysicsTests.cpp
    LABELS xvfb physics tilemap
    TIMEOUT 180
)

l2d_add_regression_test(
    Lorenzo2DAssetTests
    SOURCE Lorenzo2DAssetTests.cpp
    LABELS xvfb assets renderer
    TIMEOUT 60
)

l2d_add_regression_test(
    Lorenzo2DRendererTests
    SOURCE Lorenzo2DRendererTests.cpp
    LABELS headless renderer
    PRIVATE_INCLUDE_DIRECTORIES
        "${PROJECT_SOURCE_DIR}/src/Lorenzo2D/Renderer"
    TIMEOUT 60
)
