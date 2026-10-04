cmake_minimum_required(VERSION 4.0)
project(LuvmeDB)

include(cmake/fetch_deps.cmake)

option(LUVMEDB_LTO "Use LTO on release" ON)
option(LUVMEDB_BUILD_TESTS "Build tests" ON)
option(LUVMEDB_BUILD_BENCH "Build benchmarks" ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    option(LUVMEDB_USE_URING "Use io_uring within boost asio for async file I/O" ON)
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

# ---------------------------------------------------------------------------
# Core library: everything except main.cpp, shared by the app, tests and bench
# ---------------------------------------------------------------------------
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
        # foonathan_memory
)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND LUVMEDB_USE_URING)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(URING REQUIRED IMPORTED_TARGET liburing)

    target_link_libraries(luvmedb_core PUBLIC
            PkgConfig::URING
    )
endif ()

# ---------------------------------------------------------------------------
# Executable: thin wrapper around the core library
# ---------------------------------------------------------------------------
add_executable(LuvmeDB
        src/main.cpp
)

target_link_libraries(LuvmeDB PRIVATE
        luvmedb_core
)

# ---------------------------------------------------------------------------
# Tests and benchmarks
# ---------------------------------------------------------------------------
if (LUVMEDB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif ()

if (LUVMEDB_BUILD_BENCH)
    add_subdirectory(bench)
endif ()