add_executable(luvmedb_bench
        utils/simd.cpp
)
target_link_libraries(luvmedb_bench PRIVATE luvmedb_core benchmark::benchmark_main)