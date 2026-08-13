cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DPackageConsumer LANGUAGES CXX)

find_package(Lorenzo2D 0.11 CONFIG REQUIRED)

if(NOT TARGET Lorenzo2D::Lorenzo2D)
    message(FATAL_ERROR "Installed Lorenzo2D::Lorenzo2D target is unavailable")
endif()

add_executable(Lorenzo2DPackageConsumer main.cpp)
target_link_libraries(
    Lorenzo2DPackageConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

include(CTest)
add_test(
    NAME Lorenzo2DPackageConsumer
    COMMAND Lorenzo2DPackageConsumer
)
