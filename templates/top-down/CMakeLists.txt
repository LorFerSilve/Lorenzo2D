cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DTopDownGame LANGUAGES CXX)

find_package(Lorenzo2D 0.11 CONFIG REQUIRED)

add_executable(TopDownGame src/main.cpp)
target_link_libraries(TopDownGame PRIVATE Lorenzo2D::Lorenzo2D)
