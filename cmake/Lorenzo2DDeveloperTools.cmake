function(l2d_enable_clang_tidy target)
    if(NOT L2D_ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(L2D_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    set_property(
        TARGET ${target}
        PROPERTY CXX_CLANG_TIDY
            "${L2D_CLANG_TIDY_EXECUTABLE};--config-file=${PROJECT_SOURCE_DIR}/.clang-tidy"
    )
endfunction()

function(l2d_enable_coverage target)
    if(NOT L2D_ENABLE_COVERAGE)
        return()
    endif()

    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        message(FATAL_ERROR
            "L2D_ENABLE_COVERAGE requires GCC, Clang, or AppleClang"
        )
    endif()

    target_compile_options(${target} PRIVATE -O0 -g --coverage)
    target_link_options(${target} PRIVATE --coverage)
endfunction()

function(l2d_add_coverage_target)
    if(NOT L2D_ENABLE_COVERAGE)
        return()
    endif()

    find_program(L2D_GCOVR_EXECUTABLE NAMES gcovr REQUIRED)

    add_custom_target(coverage
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${PROJECT_BINARY_DIR}/coverage"
        COMMAND "${L2D_GCOVR_EXECUTABLE}"
            --root "${PROJECT_SOURCE_DIR}"
            --filter "${PROJECT_SOURCE_DIR}/include/Lorenzo2D"
            --filter "${PROJECT_SOURCE_DIR}/src/Lorenzo2D"
            --exclude "${PROJECT_SOURCE_DIR}/tests"
            --exclude "${PROJECT_BINARY_DIR}"
            --print-summary
            --html-details "${PROJECT_BINARY_DIR}/coverage/index.html"
            --xml "${PROJECT_BINARY_DIR}/coverage/coverage.xml"
            --xml-pretty
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        COMMENT "Generating Lorenzo2D coverage reports"
        VERBATIM
    )
endfunction()

function(l2d_add_format_targets)
    if(NOT PROJECT_IS_TOP_LEVEL)
        return()
    endif()

    find_program(L2D_CLANG_FORMAT_EXECUTABLE NAMES clang-format)

    if(NOT L2D_CLANG_FORMAT_EXECUTABLE)
        message(STATUS "clang-format not found; format targets are unavailable")
        return()
    endif()

    add_custom_target(format
        COMMAND ${CMAKE_COMMAND}
            -DL2D_CLANG_FORMAT_EXECUTABLE=${L2D_CLANG_FORMAT_EXECUTABLE}
            -DL2D_FORMAT_MODE=write
            -DL2D_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -P "${PROJECT_SOURCE_DIR}/cmake/RunClangFormat.cmake"
        COMMENT "Formatting Lorenzo2D C++ sources"
        VERBATIM
    )

    add_custom_target(format-check
        COMMAND ${CMAKE_COMMAND}
            -DL2D_CLANG_FORMAT_EXECUTABLE=${L2D_CLANG_FORMAT_EXECUTABLE}
            -DL2D_FORMAT_MODE=check
            -DL2D_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -P "${PROJECT_SOURCE_DIR}/cmake/RunClangFormat.cmake"
        COMMENT "Checking Lorenzo2D C++ formatting"
        VERBATIM
    )
endfunction()
