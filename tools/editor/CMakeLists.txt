cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DEditor VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

find_package(Lorenzo2D 1.1 CONFIG REQUIRED)

option(
    L2D_EDITOR_BUILD_TESTS
    "Build the Lorenzo2D editor regression tests"
    ON
)
option(
    L2D_EDITOR_WARNINGS_AS_ERRORS
    "Treat Lorenzo2D editor compiler warnings as errors"
    OFF
)

function(l2d_editor_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(L2D_EDITOR_WARNINGS_AS_ERRORS)
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
        if(L2D_EDITOR_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

add_library(Lorenzo2DEditorCore STATIC
    src/ComponentInspectorModel.cpp
    src/EditorCommandHistory.cpp
    src/EditorDocument.cpp
    src/SceneHierarchyModel.cpp
)
add_library(Lorenzo2DEditor::Core ALIAS Lorenzo2DEditorCore)

target_include_directories(Lorenzo2DEditorCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_features(Lorenzo2DEditorCore PUBLIC cxx_std_17)
target_link_libraries(Lorenzo2DEditorCore PUBLIC Lorenzo2D::Lorenzo2D)
l2d_editor_enable_warnings(Lorenzo2DEditorCore)

add_executable(Lorenzo2DEditor src/main.cpp)
target_compile_features(Lorenzo2DEditor PRIVATE cxx_std_17)
target_link_libraries(Lorenzo2DEditor PRIVATE Lorenzo2DEditor::Core)
l2d_editor_enable_warnings(Lorenzo2DEditor)

if(L2D_EDITOR_BUILD_TESTS)
    enable_testing()

    add_executable(
        Lorenzo2DEditorFoundationTests
        tests/EditorFoundationTests.cpp
    )
    target_compile_features(Lorenzo2DEditorFoundationTests PRIVATE cxx_std_17)
    target_link_libraries(Lorenzo2DEditorFoundationTests PRIVATE Lorenzo2DEditor::Core)
    l2d_editor_enable_warnings(Lorenzo2DEditorFoundationTests)

    add_test(
        NAME Lorenzo2DEditorFoundationTests
        COMMAND Lorenzo2DEditorFoundationTests
    )
    set_tests_properties(
        Lorenzo2DEditorFoundationTests
        PROPERTIES
            TIMEOUT 30
    )

    add_executable(
        Lorenzo2DEditorComponentInspectorTests
        tests/ComponentInspectorTests.cpp
    )
    target_compile_features(Lorenzo2DEditorComponentInspectorTests PRIVATE cxx_std_17)
    target_link_libraries(Lorenzo2DEditorComponentInspectorTests PRIVATE Lorenzo2DEditor::Core)
    l2d_editor_enable_warnings(Lorenzo2DEditorComponentInspectorTests)

    add_test(
        NAME Lorenzo2DEditorComponentInspectorTests
        COMMAND Lorenzo2DEditorComponentInspectorTests
    )
    set_tests_properties(
        Lorenzo2DEditorComponentInspectorTests
        PROPERTIES
            TIMEOUT 30
    )
endif()
