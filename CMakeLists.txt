cmake_minimum_required(VERSION 4.0)
project(LuvmeDB)

option(LUVMEDB_LTO "Use LTO on release" ON)
option(LUVMEDB_USE_URING "Use io_uring within boost asio for async file I/O" ON)
option(LUVMEDB_USE_MIMALLOC "Use mimalloc as allocator instead of malloc" ON)
option(LUVMEDB_BUILD_TESTS "Build tests" ON)
option(LUVMEDB_BUILD_BENCH "Build benchmarks" ON)
option(LUVMEDB_FETCH_DEPS "Fetch dependencies with CPM.cmake" ON)

if (LUVMEDB_USE_MIMALLOC)
    add_compile_definitions(LUVMEDB_USE_MIMALLOC)
endif ()

if (LUVMEDB_FETCH_DEPS)
    include(cmake/fetch_deps.cmake)
endif ()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux"
        AND LUVMEDB_USE_URING)
    message("Linux detected, enabling io_uring, make sure you have liburing installed")
    add_compile_definitions(BOOST_ASIO_HAS_IO_URING)
endif ()

if (LUVMEDB_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_ok OUTPUT ipo_msg LANGUAGES CXX)

    if (ipo_ok)
        message("LTO wanted and supported, enabling it on release")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    else ()
        message(WARNING "LTO wanted but not supported")
    endif ()
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(luvmedb_core STATIC
        src/pch.hpp
        src/utils/simd/strings.cpp
        src/utils/simd/strings.hpp
        src/utils/memory/bytes.cpp
        src/utils/memory/bytes.hpp
        src/storage/physical_pages.cpp
        src/storage/physical_pages.hpp
        src/dev/log.hpp
        src/dev/log.cpp
        src/storage/page_view.cpp
        src/storage/page_view.hpp
        src/storage/page_allocator.cpp
        src/storage/page_allocator.hpp
)

target_include_directories(luvmedb_core PUBLIC
        src
)

target_precompile_headers(luvmedb_core PRIVATE
        src/pch.hpp
)

target_link_libraries(luvmedb_core PUBLIC
        Boost::asio
        Boost::headers # For LEAF as it's header only
        Boost::beast
        Boost::unordered
        Boost::leaf
        glaze::glaze
        hwy
        hwy_contrib
        quill
        fmt
        flux::flux
        # foonathan_memory
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LUVMEDB_USE_URING)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(URING REQUIRED IMPORTED_TARGET liburing)

    target_link_libraries(luvmedb_core PUBLIC
            PkgConfig::URING
    )
endif ()

add_executable(LuvmeDB
        src/main.cpp
)

target_link_libraries(LuvmeDB PRIVATE
        luvmedb_core
)

# mimalloc
if (LUVMEDB_USE_MIMALLOC)
    target_link_libraries(luvmedb_core PUBLIC
            mimalloc
    )

    target_link_libraries(LuvmeDB PRIVATE
            mimalloc
   )
endif ()

if (LUVMEDB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (LUVMEDB_BUILD_BENCH)
    add_subdirectory(bench)
endif ()