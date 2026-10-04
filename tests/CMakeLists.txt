add_executable(luvmedb_tests
        utils/simd_tests.cpp
        util.hpp
        storage/page_allocator_tests.cpp
)
target_link_libraries(luvmedb_tests PRIVATE luvmedb_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(luvmedb_tests)