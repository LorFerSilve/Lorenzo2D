cmake_minimum_required(VERSION 3.28)

project(MyLorenzo2DPlatformer LANGUAGES CXX)

find_package(Lorenzo2D 0.13 CONFIG REQUIRED)

add_executable(MyLorenzo2DPlatformer src/main.cpp)
target_compile_features(MyLorenzo2DPlatformer PRIVATE cxx_std_17)
target_link_libraries(MyLorenzo2DPlatformer PRIVATE Lorenzo2D::Lorenzo2D)
