if(NOT DEFINED L2D_CLANG_FORMAT_EXECUTABLE)
    message(FATAL_ERROR "L2D_CLANG_FORMAT_EXECUTABLE is required")
endif()

if(NOT DEFINED L2D_SOURCE_DIR)
    message(FATAL_ERROR "L2D_SOURCE_DIR is required")
endif()

if(NOT L2D_FORMAT_MODE MATCHES "^(write|check)$")
    message(FATAL_ERROR "L2D_FORMAT_MODE must be write or check")
endif()

file(GLOB_RECURSE l2d_format_files
    "${L2D_SOURCE_DIR}/benchmarks/*.cpp"
    "${L2D_SOURCE_DIR}/benchmarks/*.hpp"
    "${L2D_SOURCE_DIR}/include/*.hpp"
    "${L2D_SOURCE_DIR}/sandbox/*.cpp"
    "${L2D_SOURCE_DIR}/sandbox/*.hpp"
    "${L2D_SOURCE_DIR}/src/*.cpp"
    "${L2D_SOURCE_DIR}/src/*.hpp"
    "${L2D_SOURCE_DIR}/tests/*.cpp"
    "${L2D_SOURCE_DIR}/tests/*.hpp"
    "${L2D_SOURCE_DIR}/tools/editor/*.cpp"
    "${L2D_SOURCE_DIR}/tools/editor/*.hpp"
)

set(l2d_format_failures)

foreach(l2d_file IN LISTS l2d_format_files)
    if(L2D_FORMAT_MODE STREQUAL "write")
        execute_process(
            COMMAND "${L2D_CLANG_FORMAT_EXECUTABLE}" -i "${l2d_file}"
            RESULT_VARIABLE l2d_result
        )
    else()
        execute_process(
            COMMAND "${L2D_CLANG_FORMAT_EXECUTABLE}"
                --dry-run --Werror "${l2d_file}"
            RESULT_VARIABLE l2d_result
            OUTPUT_QUIET
            ERROR_QUIET
        )
    endif()

    if(NOT l2d_result EQUAL 0)
        list(APPEND l2d_format_failures "${l2d_file}")

        if(L2D_FORMAT_MODE STREQUAL "check")
            set(l2d_formatted_file "${l2d_file}.l2d-format-canonical")
            execute_process(
                COMMAND "${L2D_CLANG_FORMAT_EXECUTABLE}" "${l2d_file}"
                OUTPUT_FILE "${l2d_formatted_file}"
                RESULT_VARIABLE l2d_format_result
            )
            if(l2d_format_result EQUAL 0)
                execute_process(
                    COMMAND git diff --no-index --no-color -- "${l2d_file}" "${l2d_formatted_file}"
                    WORKING_DIRECTORY "${L2D_SOURCE_DIR}"
                    RESULT_VARIABLE l2d_diff_result
                    OUTPUT_VARIABLE l2d_diff
                    ERROR_VARIABLE l2d_diff_error
                )
                if(l2d_diff_result EQUAL 1)
                    message(STATUS "Canonical clang-format diff for ${l2d_file}:\n${l2d_diff}")
                elseif(NOT l2d_diff_result EQUAL 0)
                    message(STATUS "Could not produce formatter diff for ${l2d_file}: ${l2d_diff_error}")
                endif()
            endif()
            file(REMOVE "${l2d_formatted_file}")
        endif()
    endif()
endforeach()

if(l2d_format_failures)
    list(JOIN l2d_format_failures "\n  " l2d_failure_list)
    message(FATAL_ERROR "clang-format failed for:\n  ${l2d_failure_list}")
endif()
