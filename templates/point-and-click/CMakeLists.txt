cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DPointAndClickGame LANGUAGES CXX)

find_package(Lorenzo2D 0.13 CONFIG REQUIRED)

add_executable(PointAndClickGame src/main.cpp)
target_link_libraries(PointAndClickGame PRIVATE Lorenzo2D::Lorenzo2D)
