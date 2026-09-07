add_executable(
    Lorenzo2DBenchmarks
    Lorenzo2DBenchmarks.cpp
)

target_link_libraries(Lorenzo2DBenchmarks PRIVATE Lorenzo2D::Lorenzo2D)
l2d_enable_warnings(Lorenzo2DBenchmarks)
l2d_enable_sanitizers(Lorenzo2DBenchmarks)
l2d_enable_coverage(Lorenzo2DBenchmarks)
l2d_enable_clang_tidy(Lorenzo2DBenchmarks)

add_executable(
    Lorenzo2DIsometricBenchmarks
    Lorenzo2DIsometricBenchmarks.cpp
)

target_link_libraries(Lorenzo2DIsometricBenchmarks PRIVATE Lorenzo2D::Lorenzo2D)
l2d_enable_warnings(Lorenzo2DIsometricBenchmarks)
l2d_enable_sanitizers(Lorenzo2DIsometricBenchmarks)
l2d_enable_coverage(Lorenzo2DIsometricBenchmarks)
l2d_enable_clang_tidy(Lorenzo2DIsometricBenchmarks)

add_executable(
    Lorenzo2DPhase10Benchmarks
    Lorenzo2DPhase10Benchmarks.cpp
)

target_link_libraries(Lorenzo2DPhase10Benchmarks PRIVATE Lorenzo2D::Lorenzo2D)
l2d_enable_warnings(Lorenzo2DPhase10Benchmarks)
l2d_enable_sanitizers(Lorenzo2DPhase10Benchmarks)
l2d_enable_coverage(Lorenzo2DPhase10Benchmarks)
l2d_enable_clang_tidy(Lorenzo2DPhase10Benchmarks)
