cmake_minimum_required(VERSION 3.28)

project(Lorenzo2DPackageConsumer LANGUAGES CXX)

find_package(Lorenzo2D 1.0 CONFIG REQUIRED)

if(NOT TARGET Lorenzo2D::Lorenzo2D)
    message(FATAL_ERROR "Installed Lorenzo2D::Lorenzo2D target is unavailable")
endif()

add_executable(Lorenzo2DPackageConsumer main.cpp)
target_link_libraries(
    Lorenzo2DPackageConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

add_executable(Lorenzo2DAssetMetadataPackageConsumer asset_metadata.cpp)
target_link_libraries(
    Lorenzo2DAssetMetadataPackageConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

add_executable(Lorenzo2DAssetFileCookerPackageConsumer asset_file_cooker.cpp)
target_link_libraries(
    Lorenzo2DAssetFileCookerPackageConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

add_executable(Lorenzo2DAssetTextureAtlasPackageConsumer asset_texture_atlas.cpp)
target_link_libraries(
    Lorenzo2DAssetTextureAtlasPackageConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

add_executable(Lorenzo2DAssetManifestRuntimePackageConsumer asset_manifest_runtime.cpp)
target_link_libraries(
    Lorenzo2DAssetManifestRuntimePackageConsumer
    PRIVATE Lorenzo2D::Lorenzo2D
)

include(CTest)
add_test(
    NAME Lorenzo2DPackageConsumer
    COMMAND Lorenzo2DPackageConsumer
)
add_test(
    NAME Lorenzo2DAssetMetadataPackageConsumer
    COMMAND Lorenzo2DAssetMetadataPackageConsumer
)
add_test(
    NAME Lorenzo2DAssetFileCookerPackageConsumer
    COMMAND Lorenzo2DAssetFileCookerPackageConsumer
)
add_test(
    NAME Lorenzo2DAssetTextureAtlasPackageConsumer
    COMMAND Lorenzo2DAssetTextureAtlasPackageConsumer
)
add_test(
    NAME Lorenzo2DAssetManifestRuntimePackageConsumer
    COMMAND Lorenzo2DAssetManifestRuntimePackageConsumer
)
