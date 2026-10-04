include(cmake/CPM.cmake)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message("Linux detected, enabling io_uring")
    message("Make sure your kernel supports it and you have it installed")
    add_compile_definitions(BOOST_ASIO_HAS_IO_URING)
endif ()

CPMAddPackage(
        NAME Boost
        VERSION 1.92.0 # Versions less than 1.85.0 may need patches for installation targets.
        URL https://github.com/boostorg/boost/releases/download/boost-1.92.0/boost-1.92.0-cmake.tar.xz
        URL_HASH SHA256=9bed76128d4e46755dbe818487788c6fceb6f72b378f4daa49b7e1e600d9088d
        OPTIONS "BOOST_ENABLE_CMAKE ON" "BOOST_SKIP_INSTALL_RULES ON" # Set `OFF` for installation
        "BUILD_SHARED_LIBS OFF" "BOOST_INCLUDE_LIBRARIES beast\\\;asio\\\;unordered\\\;leaf" # Note the escapes!
)
CPMAddPackage("gh:stephenberry/glaze@9.0.0")
CPMAddPackage("gh:foonathan/memory@0.7-4")
CPMAddPackage(
        NAME highway
        GITHUB_REPOSITORY google/highway
        GIT_TAG 1.4.0
        OPTIONS
        "HWY_ENABLE_TESTS OFF"
        "HWY_ENABLE_EXAMPLES OFF"
        "HWY_ENABLE_CONTRIB ON"
)
CPMAddPackage("gh:google/googletest@1.18.0")
CPMAddPackage("gh:google/benchmark@1.9.5")
CPMAddPackage("gh:odygrd/quill@13.0.0")
# yes i know the creator of fmt is a soyboy
# but std::format is a bit slower :(
CPMAddPackage("gh:fmtlib/fmt#12.2.0")
