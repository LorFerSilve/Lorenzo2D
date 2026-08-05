add_executable(
    Lorenzo2DBenchmarks
    Lorenzo2DBenchmarks.cpp
)

target_link_libraries(Lorenzo2DBenchmarks PRIVATE Lorenzo2D::Lorenzo2D)
l2d_enable_warnings(Lorenzo2DBenchmarks)
l2d_enable_sanitizers(Lorenzo2DBenchmarks)
l2d_enable_coverage(Lorenzo2DBenchmarks)
l2d_enable_clang_tidy(Lorenzo2DBenchmarks)
