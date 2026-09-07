cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DSubprojectConsumer LANGUAGES CXX)

if(NOT DEFINED L2D_SOURCE_DIR)
    message(FATAL_ERROR "L2D_SOURCE_DIR must point to the Lorenzo2D source tree")
endif()

add_subdirectory("${L2D_SOURCE_DIR}" Lorenzo2D)

if(L2D_BUILD_SANDBOX)
    message(FATAL_ERROR "L2D_BUILD_SANDBOX must default to OFF in dependency mode")
endif()

if(L2D_BUILD_TESTS)
    message(FATAL_ERROR "L2D_BUILD_TESTS must default to OFF in dependency mode")
endif()

if(L2D_BUILD_EXAMPLES)
    message(FATAL_ERROR "L2D_BUILD_EXAMPLES must default to OFF in dependency mode")
endif()

if(L2D_INSTALL)
    message(FATAL_ERROR "L2D_INSTALL must default to OFF in dependency mode")
endif()

foreach(unexpected_target
    Sandbox
    Lorenzo2DMinimalExample
    Lorenzo2DAnimationExample
    Lorenzo2DPhysicsExample
    Lorenzo2DPhase4Example
    Lorenzo2DPhase5Example
    Lorenzo2DPhase6Example
    Lorenzo2DPhase7Example
    Lorenzo2DPhase8Example
    Lorenzo2DPhase9Example
    Lorenzo2DIsometricTests
    Lorenzo2DTests
    Lorenzo2DTimingAccountingTests
    Lorenzo2DPhysicsTests
    Lorenzo2DAssetTests
    Lorenzo2DRendererTests
)
    if(TARGET ${unexpected_target})
        message(FATAL_ERROR
            "Unexpected first-party target in dependency mode: ${unexpected_target}"
        )
    endif()
endforeach()

if(NOT TARGET Lorenzo2D::Lorenzo2D)
    message(FATAL_ERROR "Lorenzo2D::Lorenzo2D is unavailable")
endif()

add_executable(Lorenzo2DSubprojectConsumer main.cpp)
target_link_libraries(
    Lorenzo2DSubprojectConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

include(CTest)
add_test(
    NAME Lorenzo2DSubprojectConsumer
    COMMAND Lorenzo2DSubprojectConsumer
)
