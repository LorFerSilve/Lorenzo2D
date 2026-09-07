cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DIsometricGame LANGUAGES CXX)

find_package(Lorenzo2D 1.0 CONFIG REQUIRED)

add_executable(IsometricGame src/main.cpp)
target_compile_features(IsometricGame PRIVATE cxx_std_17)
target_link_libraries(IsometricGame PRIVATE Lorenzo2D::Lorenzo2D)
